package app.zxtune.ui.browser;

import android.app.Application;
import android.net.Uri;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModelProviders;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.Future;
import java.util.concurrent.atomic.AtomicReference;

import app.zxtune.Log;
import app.zxtune.analytics.Analytics;
import app.zxtune.fs.provider.VfsProviderClient;

/*
TODO:
 - icons for files
 - per-item progress and error status
 - parallel browsing
*/

// public for provider
public class Model extends AndroidViewModel {

  private static final String TAG = Model.class.getName();

  interface Client {
    void onFileBrowse(Uri uri);

    void onError(String msg);
  }

  class State {
    Uri uri;
    String query;
    List<BreadcrumbsEntry> breadcrumbs;
    List<ListingEntry> entries;
  }

  private final ExecutorService async = Executors.newSingleThreadExecutor();
  private final MutableLiveData<State> state = new MutableLiveData<>();
  private final MutableLiveData<Integer> progress = new MutableLiveData<>();
  private final AtomicReference<Future<?>> lastTask = new AtomicReference<>();
  private Client client = new Client() {
    @Override
    public void onFileBrowse(Uri uri) {}

    @Override
    public void onError(String msg) {}
  };
  private final VfsProviderClient providerClient;

  static Model of(Fragment owner) {
    return ViewModelProviders.of(owner).get(Model.class);
  }

  public Model(@NonNull Application application) {
    super(application);
    providerClient = new VfsProviderClient(application);
  }

  final LiveData<State> getState() {
    return state;
  }

  final LiveData<Integer> getProgress() {
    return progress;
  }

  final void setClient(@NonNull Client client) {
    this.client = client;
  }

  final void browse(@NonNull Uri uri) {
    Analytics.sendBrowserEvent(uri, Analytics.BROWSER_ACTION_BROWSE);
    executeAsync(new BrowseTask(uri), "browse " + uri);
  }

  final void browseParent() {
    final State currentState = state.getValue();
    if (currentState != null && currentState.breadcrumbs.size() > 1) {
      Analytics.sendBrowserEvent(currentState.uri, Analytics.BROWSER_ACTION_BROWSE_PARENT);
      executeAsync(new BrowseParentTask(currentState.breadcrumbs), "browse parent");
    } else {
      Analytics.sendBrowserEvent(Uri.EMPTY, Analytics.BROWSER_ACTION_BROWSE_PARENT);
      executeAsync(new BrowseTask(Uri.EMPTY), "browse root");
    }
  }

  final void reload() {
    final State currentState = state.getValue();
    if (currentState != null) {
      browse(currentState.uri);
    }
  }

  final void search(@NonNull String query) {
    final State currentState = state.getValue();
    if (currentState != null) {
      Analytics.sendBrowserEvent(currentState.uri, Analytics.BROWSER_ACTION_SEARCH);
      executeAsync(new SearchTask(currentState, query),
          "search for " + query + " in " + currentState.uri);
    }
  }

  private interface AsyncOperation {
    void execute() throws Exception;
  }

  private class AsyncOperationWrapper implements Runnable {

    private final AsyncOperation op;
    private final String descr;

    AsyncOperationWrapper(AsyncOperation op, String descr) {
      this.op = op;
      this.descr = descr;
    }

    @Override
    public void run() {
      try {
        Log.d(TAG, "Started %s", descr);
        progress.postValue(-1);
        op.execute();
      } catch (InterruptedException e) {
        Log.d(TAG, "Cancelled %s", descr);
      } catch (Exception e) {
        reportError(e);
      } finally {
        progress.postValue(null);
        Log.d(TAG, "Finished %s", descr);
      }
    }
  }

  private void executeAsync(@NonNull AsyncOperation op, String descr) {
    Future<?> prev;
    synchronized(lastTask) {
      prev = lastTask.getAndSet(async.submit(new AsyncOperationWrapper(op, descr)));
    }
    if (prev != null) {
      prev.cancel(true);
    }
  }

  private void updateProgress(int done, int total) {
    progress.postValue(done == -1 || total == 0 ? done : 100 * done / total);
  }

  private class BrowseTask implements AsyncOperation {

    private final Uri uri;

    BrowseTask(@NonNull Uri uri) {
      this.uri = uri;
    }

    @Override
    public void execute() throws Exception {
      final ObjectType type = resolve(uri);
      if (ObjectType.FILE == type || ObjectType.DIR_WITH_FEED == type) {
        client.onFileBrowse(uri);
      } else if (ObjectType.DIR == type) {
        final State newState = new State();
        newState.uri = uri;
        newState.entries = getEntries(uri);
        newState.breadcrumbs = getParents();
        state.postValue(newState);
      }
    }

    private List<BreadcrumbsEntry> getParents() throws Exception {
      final ArrayList<BreadcrumbsEntry> result = new ArrayList<>();
      providerClient.parents(uri, new VfsProviderClient.ParentsCallback() {
        @Override
        public void onObject(Uri uri, String name, int icon) {
          final BreadcrumbsEntry entry = new BreadcrumbsEntry();
          entry.uri = uri;
          entry.title = name;
          entry.icon = icon;
          result.add(entry);
        }

        @Override
        public void onProgress(int done, int total) {
          updateProgress(done, total);
        }
      });
      return result;
    }
  }

  class BrowseParentTask implements AsyncOperation {

    private final List<BreadcrumbsEntry> items;

    BrowseParentTask(List<BreadcrumbsEntry> items) {
      this.items = items;
    }

    @Override
    public void execute() throws Exception {
      for (int idx = items.size() - 2; idx >= 0; --idx) {
        if (tryBrowseAt(idx)) {
          break;
        }
      }
    }

    private boolean tryBrowseAt(int idx) throws InterruptedException {
      final Uri uri = items.get(idx).uri;
      try {
        final ObjectType type = resolve(uri);
        if (ObjectType.DIR == type || ObjectType.DIR_WITH_FEED == type) {
          final State newState = new State();
          newState.uri = uri;
          newState.entries = getEntries(uri);
          newState.breadcrumbs = items.subList(0, idx + 1);
          state.postValue(newState);
          return true;
        }
      } catch (InterruptedException e) {
        throw e;
      } catch (Exception e) {
        Log.d(TAG, "Skipping " + uri + " while navigating up");
      }
      return false;
    }
  }

  private class SearchTask implements AsyncOperation {

    private final State newState = new State();

    SearchTask(@NonNull State state, @NonNull String query) {
      newState.uri = state.uri;
      newState.breadcrumbs = state.breadcrumbs;
      newState.entries = new ArrayList<>();
      newState.query = query;
    }

    @Override
    public void execute() throws Exception {
      state.postValue(newState);
      // assume already resolved
      providerClient.search(newState.uri, newState.query, new VfsProviderClient.ListingCallback() {
        @Override
        public void onDir(Uri uri, String name, String description, int icon, boolean hasFeed) {

        }

        @Override
        public void onFile(Uri uri, String name, String description, String details,
                           Integer tracks, Boolean cached) {
          final ListingEntry entry = createEntry(uri, name, description);
          entry.type = ListingEntry.FILE;
          entry.details = details;
          entry.tracks = tracks;
          entry.cached = cached;
          newState.entries.add(entry);
          state.postValue(newState);
        }

        @Override
        public void onProgress(int done, int total) {
        }
      });
    }
  }

  private void reportError(Exception e) {
    final Throwable cause = e.getCause();
    final String msg = cause != null ? cause.getMessage() : e.getMessage();
    client.onError(msg);
  }

  enum ObjectType {
    DIR,
    DIR_WITH_FEED,
    FILE
  }

  private ObjectType resolve(@NonNull Uri uri) throws Exception {
    final ObjectType[] result = new ObjectType[]{null};
    providerClient.resolve(uri, new VfsProviderClient.ListingCallback() {
      @Override
      public void onDir(Uri uri, String name, String description, int icon, boolean hasFeed) {
        result[0] = hasFeed ? ObjectType.DIR_WITH_FEED : ObjectType.DIR;
      }

      @Override
      public void onFile(Uri uri, String name, String description, String details, Integer tracks,
                         Boolean cached) {
        result[0] = ObjectType.FILE;
      }

      @Override
      public void onProgress(int done, int total) {
        updateProgress(done, total);
      }
    });
    return result[0];
  }

  private List<ListingEntry> getEntries(@NonNull Uri uri) throws Exception {
    final ArrayList<ListingEntry> result = new ArrayList<>();
    providerClient.list(uri, new VfsProviderClient.ListingCallback() {
      @Override
      public void onDir(Uri uri, String name, String description, int icon, boolean hasFeed) {
        final ListingEntry entry = createEntry(uri, name, description);
        entry.type = ListingEntry.FOLDER;
        entry.icon = icon;
        result.add(entry);
      }

      @Override
      public void onFile(Uri uri, String name, String description, String details, Integer tracks,
                         Boolean cached) {
        final ListingEntry entry = createEntry(uri, name, description);
        entry.type = ListingEntry.FILE;
        entry.details = details;
        entry.tracks = tracks;
        entry.cached = cached;
        result.add(entry);
      }

      @Override
      public void onProgress(int done, int total) {
        updateProgress(done, total);
      }
    });
    return result;
  }

  private static ListingEntry createEntry(Uri uri, String name, String description) {
    final ListingEntry result = new ListingEntry();
    result.uri = uri;
    result.title = name;
    result.description = description;
    return result;
  }
}
