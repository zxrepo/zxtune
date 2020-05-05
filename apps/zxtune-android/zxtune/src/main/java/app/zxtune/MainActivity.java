/**
 * @file
 * @brief Main application activity
 * @author vitamin.caig@gmail.com
 */

package app.zxtune;

import android.Manifest;
import android.app.PendingIntent;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.Observer;
import androidx.lifecycle.ViewModelProviders;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import androidx.annotation.Nullable;
import androidx.fragment.app.DialogFragment;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import android.support.v4.media.session.MediaControllerCompat;
import androidx.viewpager.widget.ViewPager;
import androidx.appcompat.app.AppCompatActivity;
import android.util.SparseArray;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Toast;
import app.zxtune.analytics.Analytics;
import app.zxtune.models.MediaSessionConnection;
import app.zxtune.models.MediaSessionModel;
import app.zxtune.ui.AboutFragment;
import app.zxtune.ui.browser.BrowserFragment;
import app.zxtune.ui.NowPlayingFragment;
import app.zxtune.ui.PlaylistFragment;
import app.zxtune.ui.ViewPagerAdapter;

public class MainActivity extends AppCompatActivity {

  public interface PagerTabListener {
    void onTabVisibilityChanged(boolean isVisible);
  }

  private static class StubPagerTabListener implements PagerTabListener {
    @Override
    public void onTabVisibilityChanged(boolean isVisible) {}

    public static PagerTabListener instance() {
      return Holder.INSTANCE;
    }

    //onDemand holder idiom
    private static class Holder {
      public static final PagerTabListener INSTANCE = new StubPagerTabListener();
    }

  }

  private static final int NO_PAGE = -1;
  private ViewPager pager;
  private int browserPageIndex;
  private BrowserFragment browser;
  private MediaSessionConnection sessionConnection;

  public static PendingIntent createPendingIntent(Context ctx) {
    final Intent intent = new Intent(ctx, MainActivity.class);
    intent.setFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP);
    return PendingIntent.getActivity(ctx, 0, intent, 0);
  }

  public MainActivity() {
    super(R.layout.main_activity);
  }

  @Override
  public void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    fillPages();
    if (Build.VERSION.SDK_INT >= 16) {
      Permission.request(this, Manifest.permission.READ_EXTERNAL_STORAGE);
    }
    Permission.request(this, Manifest.permission.WRITE_EXTERNAL_STORAGE);

    sessionConnection = new MediaSessionConnection(this);

    if (savedInstanceState == null) {
      subscribeForPendingOpenRequest();
    }
    Analytics.sendUiEvent(Analytics.UI_ACTION_OPEN);
  }

  @Override
  public void onDestroy() {
    super.onDestroy();

    Analytics.sendUiEvent(Analytics.UI_ACTION_CLOSE);
  }

  @Override
  public void onStart() {
    super.onStart();
    sessionConnection.connect();
  }

  @Override
  public void onStop() {
    super.onStop();
    sessionConnection.disconnect();
  }

  @Override
  public boolean onCreateOptionsMenu(Menu menu) {
    super.onCreateOptionsMenu(menu);
    getMenuInflater().inflate(R.menu.main, menu);
    return true;
  }

  @Override
  public boolean onOptionsItemSelected(MenuItem item) {
    switch (item.getItemId()) {
      case R.id.action_prefs:
        showPreferences();
        break;
      case R.id.action_about:
        showAbout();
        break;
      case R.id.action_rate:
        rateApplication();
        break;
      case R.id.action_quit:
        quit();
        break;
      default:
        return super.onOptionsItemSelected(item);
    }
    return true;
  }

  @Override
  public void onBackPressed() {
    if (pager != null && pager.getCurrentItem() == browserPageIndex) {
      browser.moveUp();
    } else {
      super.onBackPressed();
    }
  }

  private void subscribeForPendingOpenRequest() {
    final Intent intent = getIntent();
    if (intent != null && Intent.ACTION_VIEW.equals(intent.getAction())) {
      final Uri uri = intent.getData();
      if (uri != null) {
        final MediaSessionModel model = ViewModelProviders.of(this).get(MediaSessionModel.class);
        final LiveData<MediaControllerCompat> ctrl = model.getMediaController();
        ctrl.observe(this,
            new Observer<MediaControllerCompat>() {
              @Override
              public void onChanged(@Nullable MediaControllerCompat mediaControllerCompat) {
                if (mediaControllerCompat != null) {
                  mediaControllerCompat.getTransportControls().playFromUri(uri, null);
                  ctrl.removeObserver(this);
                }
              }
            });
      }
    }
  }

  private void fillPages() {
    final FragmentManager manager = getSupportFragmentManager();
    final FragmentTransaction transaction = manager.beginTransaction();
    if (null == manager.findFragmentById(R.id.now_playing)) {
      transaction.replace(R.id.now_playing, NowPlayingFragment.createInstance());
    }
    browser = (BrowserFragment) manager.findFragmentById(R.id.browser_view);
    if (null == browser) {
      browser = BrowserFragment.createInstance();
      transaction.replace(R.id.browser_view, browser);
    }
    if (null == manager.findFragmentById(R.id.playlist_view)) {
      transaction.replace(R.id.playlist_view, PlaylistFragment.createInstance());
    }
    transaction.commit();
    setupViewPager();
  }

  private void setupViewPager() {
    browserPageIndex = NO_PAGE;
    pager = findViewById(R.id.view_pager);
    if (null != pager) {
      final ViewPagerAdapter adapter = new ViewPagerAdapter(pager);
      pager.setAdapter(adapter);
      browserPageIndex = adapter.getCount() - 1;
      while (browserPageIndex >= 0 && ((View) adapter.instantiateItem(pager, browserPageIndex)).getId() != R.id.browser_view) {
        --browserPageIndex;
      }
      pager.addOnPageChangeListener(new ViewPager.OnPageChangeListener() {

        private SparseArray<PagerTabListener> listeners = new SparseArray<>();
        private int prevPos = pager.getCurrentItem();

        @Override
        public void onPageScrolled(int position, float positionOffset, int positionOffsetPixels) {
        }

        @Override
        public void onPageSelected(int newPos) {
          getListener(prevPos).onTabVisibilityChanged(false);
          getListener(newPos).onTabVisibilityChanged(true);
          prevPos = newPos;
        }

        @Override
        public void onPageScrollStateChanged(int state) {
        }

        private PagerTabListener getListener(int idx) {
          PagerTabListener result = listeners.get(idx);
          if (result == null) {
            result = createListener(idx);
            listeners.put(idx, result);
          }
          return result;
        }

        private PagerTabListener createListener(int idx) {
          final int id = ((View) adapter.instantiateItem(pager, idx)).getId();
          final Fragment frag = getSupportFragmentManager().findFragmentById(id);
          if (frag instanceof PagerTabListener) {
            return (PagerTabListener) frag;
          } else {
            return StubPagerTabListener.instance();
          }
        }
      });
    }
  }

  private void showPreferences() {
    final Intent intent = new Intent(this, PreferencesActivity.class);
    startActivity(intent);
    Analytics.sendUiEvent(Analytics.UI_ACTION_PREFERENCES);
  }

  private void rateApplication() {
    final Intent intent = new Intent(Intent.ACTION_VIEW);
    intent.setData(Uri.parse("market://details?id=" + getPackageName()));
    if (safeStartActivity(intent)) {
      Analytics.sendUiEvent(Analytics.UI_ACTION_RATE);
    } else {
      intent.setData(Uri.parse("https://play.google.com/store/apps/details?id=" + getPackageName()));
      if (safeStartActivity(intent)) {
        Analytics.sendUiEvent(Analytics.UI_ACTION_RATE);
      } else {
        Toast.makeText(this, "Error", Toast.LENGTH_LONG).show();
      }
    }
  }

  private boolean safeStartActivity(Intent intent) {
    try {
      startActivity(intent);
      return true;
    } catch (ActivityNotFoundException e) {
      return false;
    }
  }

  private void showAbout() {
    final DialogFragment fragment = AboutFragment.createInstance();
    fragment.show(getSupportFragmentManager(), "about");
    Analytics.sendUiEvent(Analytics.UI_ACTION_ABOUT);
  }

  private void quit() {
    Analytics.sendUiEvent(Analytics.UI_ACTION_QUIT);
    final Intent intent = MainService.createIntent(this, null);
    stopService(intent);
    finish();
  }
}
