/**
 * @file
 * @brief Scanning service
 * @author vitamin.caig@gmail.com
 */

package app.zxtune;

import android.app.IntentService;
import android.app.PendingIntent;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Handler;
import android.os.Parcelable;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.support.v4.os.CancellationSignal;
import android.support.v4.os.OperationCanceledException;
import android.widget.Toast;
import app.zxtune.core.Identifier;
import app.zxtune.core.Module;
import app.zxtune.core.Scanner;
import app.zxtune.device.ui.Notifications;
import app.zxtune.playlist.Item;
import app.zxtune.playlist.PlaylistQuery;

import java.util.concurrent.atomic.AtomicInteger;

public class ScanService extends IntentService {

  private static final String TAG = ScanService.class.getName();

  public static final String ACTION_START = TAG + ".start";
  public static final String ACTION_CANCEL = TAG + ".cancel";
  public static final String EXTRA_PATHS = "paths";

  private final Handler handler;
  private final NotifyTask tracking;
  private final AtomicInteger addedItems;
  private CancellationSignal signal;
  private Exception error;

  //TODO: remove C&P
  public static void add(Context ctx, app.zxtune.playback.Item source) {
    try {
      final Item item = new app.zxtune.playlist.Item(source);
      ctx.getContentResolver().insert(PlaylistQuery.ALL, item.toContentValues());
      ctx.getContentResolver().notifyChange(PlaylistQuery.ALL, null);
      Analytics.sendPlaylistEvent("Add", 1);
    } catch (Exception error) {
      Log.w(TAG, error, "Failed to add item to playlist");
    }
  }

  public static void add(Context context, Uri[] uris) {
    final Intent intent = new Intent(context, ScanService.class);
    intent.setAction(ScanService.ACTION_START);
    intent.putExtra(ScanService.EXTRA_PATHS, uris);
    context.startService(intent);
    Analytics.sendPlaylistEvent("Add", uris.length);
  }

  /**
   * InsertThread is executed for onCreate..onDestroy interval
   */

  public ScanService() {
    super(ScanService.class.getName());
    this.handler = new Handler();
    this.tracking = new NotifyTask();
    this.addedItems = new AtomicInteger();
    this.signal = new CancellationSignal();
    setIntentRedelivery(false);
  }

  @Override
  public void onCreate() {
    super.onCreate();
    makeToast(R.string.scanning_started, Toast.LENGTH_SHORT);
  }

  @Override
  public void onStart(Intent intent, int startId) {
    if (ACTION_CANCEL.equals(intent.getAction())) {
      signal.cancel();
      stopSelf();
    } else {
      super.onStart(intent, startId);
    }
  }

  @Override
  public void onDestroy() {
    super.onDestroy();
    if (error != null) {
      final String msg = getString(R.string.scanning_failed, error.getMessage());
      makeToast(msg, Toast.LENGTH_LONG);
    } else {
      makeToast(R.string.scanning_stopped, Toast.LENGTH_SHORT);
    }
  }

  private void makeToast(int textRes, int duration) {
    makeToast(getString(textRes), duration);
  }

  private void makeToast(final String text, final int duration) {
    handler.post(new Runnable() {
      @Override
      public void run() {
        Toast.makeText(ScanService.this, text, duration).show();
      }
    });
  }

  @Override
  protected void onHandleIntent(Intent intent) {
    if (ACTION_START.equals(intent.getAction())) {
      final Parcelable[] paths = intent.getParcelableArrayExtra(EXTRA_PATHS);
      scan(paths);
    }
  }

  private void scan(Parcelable[] paths) {
    final Uri[] uris = new Uri[paths.length];
    System.arraycopy(paths, 0, uris, 0, uris.length);

    tracking.start();
    try {
      scan(uris);
    } catch (Exception e) {
      error = e;
    } finally {
      tracking.stop();
    }
  }

  private void scan(Uri[] uris) {
    final ContentResolver resolver = getContentResolver();
    for (Uri uri : uris) {
      Log.d(TAG, "scan on %s", uri);
      Scanner.analyzeIdentifier(new Identifier(uri), new Scanner.Callback() {
        @Override
        public void onModule(Identifier id, Module module) {
          signal.throwIfCanceled();
          final Item item = new Item(id, module);
          resolver.insert(PlaylistQuery.ALL, item.toContentValues());
          addedItems.incrementAndGet();
          error = null;
        }

        @Override
        public void onError(Identifier id, Exception e) {
          Log.w(TAG, e, "Error while processing " + id);
          if (e instanceof OperationCanceledException) {
            throw (OperationCanceledException) e;
          } else {
            error = e;
          }
        }
      });
    }
  }

  private class NotifyTask implements Runnable {

    private static final int NOTIFICATION_PERIOD = 2000;

    private WakeLock wakeLock;
    private StatusNotification notification;

    final void start() {
      notification = new StatusNotification();
      handler.postDelayed(this, NOTIFICATION_PERIOD);
      getWakelock().acquire();
    }

    final void stop() {
      getWakelock().release();
      handler.removeCallbacks(this);
      notifyResolver();
      notification.hide();
      notification = null;
    }

    @Override
    public void run() {
      Log.d(TAG, "Notify about changes");
      handler.postDelayed(this, NOTIFICATION_PERIOD);
      notifyResolver();
      notification.show();
    }

    private WakeLock getWakelock() {
      if (wakeLock == null) {
        final PowerManager pm = (PowerManager) getSystemService(Context.POWER_SERVICE);
        wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "ScanService");
      }
      return wakeLock;
    }

    private void notifyResolver() {
      getContentResolver().notifyChange(PlaylistQuery.ALL, null);
    }

    private class StatusNotification {

      private final CharSequence titlePrefix;
      private Notifications.Controller delegate;

      StatusNotification() {
        this.titlePrefix = getText(R.string.scanning_title);
        this.delegate = Notifications.createForService(ScanService.this, R.drawable.ic_stat_notify_scan);
        final Intent cancelIntent = new Intent(ScanService.this, ScanService.class);
        cancelIntent.setAction(ACTION_CANCEL);
        delegate.getBuilder()
            .setContentIntent(
                PendingIntent.getService(ScanService.this, 0, cancelIntent,
                    PendingIntent.FLAG_UPDATE_CURRENT)).setOngoing(true).setProgress(0, 0, true)
            .setContentTitle(titlePrefix)
            .setContentText(getText(R.string.scanning_text));
      }

      final void show() {
        final StringBuilder str = new StringBuilder();
        str.append(titlePrefix);
        str.append(" ");
        final int items = addedItems.get();
        str.append(getResources().getQuantityString(R.plurals.tracks, items, items));
        delegate.getBuilder().setContentTitle(str.toString());
        delegate.show();
      }

      final void hide() {
        delegate.hide();
      }
    }
  }
}
