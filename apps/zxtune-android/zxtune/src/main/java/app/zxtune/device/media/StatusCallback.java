package app.zxtune.device.media;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Handler;
import android.os.SystemClock;
import androidx.annotation.NonNull;
import androidx.core.content.res.ResourcesCompat;
import android.support.v4.media.MediaMetadataCompat;
import android.support.v4.media.session.MediaSessionCompat;
import android.support.v4.media.session.PlaybackStateCompat;
import android.widget.Toast;
import app.zxtune.core.Identifier;
import app.zxtune.Log;
import app.zxtune.MainApplication;
import app.zxtune.R;
import app.zxtune.Releaseable;
import app.zxtune.TimeStamp;
import app.zxtune.core.ModuleAttributes;
import app.zxtune.fs.Vfs;
import app.zxtune.fs.VfsExtensions;
import app.zxtune.fs.VfsObject;
import app.zxtune.playback.CallbackSubscription;
import app.zxtune.playback.Item;
import app.zxtune.playback.PlaybackControl;
import app.zxtune.playback.PlaybackService;
import app.zxtune.playback.Callback;

import java.util.concurrent.TimeUnit;

//! Events gate from local service to mediasession
class StatusCallback implements Callback {

  private static final String TAG = StatusCallback.class.getName();

  private final MediaSessionCompat session;
  private final PlaybackStateCompat.Builder builder;
  private final Handler handler = new Handler();

  static Releaseable subscribe(PlaybackService svc, MediaSessionCompat session) {
    final StatusCallback cb = new StatusCallback(session);
    final PlaybackControl ctrl = svc.getPlaybackControl();
    //TODO: rework repeat/shuffle model
    session.setShuffleMode(ctrl.getSequenceMode().ordinal());
    session.setRepeatMode(ctrl.getTrackMode().ordinal());
    return new CallbackSubscription(svc, cb);
  }

  private StatusCallback(MediaSessionCompat session) {
    this.session = session;
    this.builder = new PlaybackStateCompat.Builder();
    this.builder.setActions(
        PlaybackStateCompat.ACTION_SKIP_TO_PREVIOUS | PlaybackStateCompat.ACTION_PLAY_PAUSE |
            PlaybackStateCompat.ACTION_PLAY | PlaybackStateCompat.ACTION_PAUSE |
            PlaybackStateCompat.ACTION_STOP | PlaybackStateCompat.ACTION_SKIP_TO_NEXT
    );
  }

  @Override
  public void onInitialState(PlaybackControl.State state) {
    sendState(state, TimeStamp.EMPTY);
  }

  @Override
  public void onStateChanged(PlaybackControl.State state, TimeStamp pos) {
    sendState(state, pos);
  }

  private void sendState(PlaybackControl.State state, TimeStamp pos) {
    final boolean isPlaying = state == PlaybackControl.State.PLAYING;
    final int stateCompat = isPlaying
                                ? PlaybackStateCompat.STATE_PLAYING
                                : PlaybackStateCompat.STATE_STOPPED;
    final long position = pos.convertTo(TimeUnit.MILLISECONDS);
    final float speed = isPlaying ? 1 : 0;
    builder.setState(stateCompat, position, speed, SystemClock.elapsedRealtime());
    session.setPlaybackState(builder.build());
    session.setActive(isPlaying);
  }

  @Override
  public void onItemChanged(Item item) {
    try {
      final Identifier dataId = item.getDataId();
      final MediaMetadataCompat.Builder builder = new MediaMetadataCompat.Builder();
      String title = item.getTitle();
      if (title.isEmpty()) {
        title = dataId.getDisplayFilename();
      }
      builder.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_TITLE, title);
      builder.putString(MediaMetadataCompat.METADATA_KEY_TITLE, title);
      final String author = item.getAuthor();
      if (!author.isEmpty()) {
        builder.putString(MediaMetadataCompat.METADATA_KEY_DISPLAY_SUBTITLE, author);
        builder.putString(MediaMetadataCompat.METADATA_KEY_ARTIST, author);
      } else {
        // Do not localize
        builder.putString(MediaMetadataCompat.METADATA_KEY_ARTIST, "Unknown artist");
      }
      putString(builder, MediaMetadataCompat.METADATA_KEY_DISPLAY_DESCRIPTION, item.getComment());
      putString(builder, ModuleAttributes.PROGRAM, item.getProgram());
      putString(builder, ModuleAttributes.STRINGS, item.getStrings());
      builder.putLong(MediaMetadataCompat.METADATA_KEY_DURATION,
          item.getDuration().convertTo(TimeUnit.MILLISECONDS));
      builder.putBitmap(MediaMetadataCompat.METADATA_KEY_DISPLAY_ICON, getLocationIcon(dataId.getDataLocation()));
      builder.putString(MediaMetadataCompat.METADATA_KEY_MEDIA_URI, dataId.toString());
      builder.putString(MediaMetadataCompat.METADATA_KEY_MEDIA_ID, item.getId().toString());
      builder.putString(VfsExtensions.SHARE_URL, getShareUrl(dataId.getDataLocation()));
      session.setMetadata(builder.build());
    } catch (Exception e) {
      Log.w(TAG, e, "onItemChanged()");
    }
  }

  @Override
  public void onError(@NonNull final String e) {
    handler.post(new Runnable() {
      @Override
      public void run() {
        Toast.makeText(MainApplication.getInstance(), e, Toast.LENGTH_SHORT).show();
      }
    });
  }

  private static void putString(MediaMetadataCompat.Builder builder, String key, String value) {
    if (value != null && !value.isEmpty()) {
      builder.putString(key, value);
    }
  }

  private Bitmap getLocationIcon(Uri location) {
    try {
      final Resources resources = MainApplication.getInstance().getResources();
      final int id = getLocationIconResource(location);
      final Drawable drawable = ResourcesCompat.getDrawableForDensity(resources, id, 320/*XHDPI*/, null);
      if (drawable instanceof BitmapDrawable) {
        return ((BitmapDrawable) drawable).getBitmap();
      } else {
        final Bitmap result = Bitmap.createBitmap(64, 64, Bitmap.Config.ARGB_8888);
        final Canvas canvas = new Canvas(result);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return result;
      }
    } catch (Exception e) {
    }
    return null;
  }

  private int getLocationIconResource(Uri location) {
    try {
      final Uri rootLocation = new Uri.Builder().scheme(location.getScheme()).build();
      final VfsObject root = Vfs.resolve(rootLocation);
      return (Integer) root.getExtension(VfsExtensions.ICON_RESOURCE);
    } catch (Exception e) {
      return R.drawable.ic_launcher;
    }
  }

  private static String getShareUrl(Uri location) {
    try {
      final VfsObject obj = Vfs.resolve(location);
      return (String) obj.getExtension(VfsExtensions.SHARE_URL);
    } catch (Exception e) {
      return null;
    }
  }
}
