/**
 * 
 * @file
 *
 * @brief
 *
 * @author vitamin.caig@gmail.com
 * 
 */

package app.zxtune;

import android.app.PendingIntent;
import android.content.Context;
import android.content.Intent;
import android.media.AudioManager;
import android.media.MediaMetadataRetriever;
import android.media.RemoteControlClient;
import android.media.RemoteControlClient.MetadataEditor;

import java.util.concurrent.TimeUnit;

import app.zxtune.playback.stubs.CallbackStub;
import app.zxtune.playback.CallbackSubscription;
import app.zxtune.playback.Item;
import app.zxtune.playback.PlaybackControl;
import app.zxtune.playback.PlaybackService;

//TODO: position change support starting from 18 
class RemoteControl implements Releaseable {

  private static final String TAG = RemoteControl.class.getName();
  private static final AudioManager.OnAudioFocusChangeListener HANDLER =
      new AudioManager.OnAudioFocusChangeListener() {
        @Override
        public void onAudioFocusChange(int focusChange) {
        }
      };
  private final AudioManager audioManager;
  private final RemoteControlClient controlClient;
  private final Releaseable callback;
  private final Releaseable focus;

  private RemoteControl(AudioManager manager, RemoteControlClient controlClient, PlaybackService svc) {
    this.audioManager = manager;
    this.controlClient = controlClient;
    this.callback = new CallbackSubscription(svc, new StatusCallback());
    this.focus = tryCaptureFocus();
    audioManager.registerRemoteControlClient(controlClient);
    controlClient.setTransportControlFlags(getTransportControlFlags());
  }

  private static int getTransportControlFlags() {
    return RemoteControlClient.FLAG_KEY_MEDIA_NEXT
        | RemoteControlClient.FLAG_KEY_MEDIA_PLAY_PAUSE
        | RemoteControlClient.FLAG_KEY_MEDIA_PREVIOUS;
  }

  @Override
  public void release() {
    audioManager.unregisterRemoteControlClient(controlClient);
    focus.release();
    callback.release();
  }

  public static Releaseable subscribe(Context context, PlaybackService svc) {
    final Intent mediaButtonIntent = new Intent(Intent.ACTION_MEDIA_BUTTON);
    mediaButtonIntent.setComponent(MediaButtonsHandler.getName(context));
    final PendingIntent mediaPendingIntent = PendingIntent.getBroadcast(
        context.getApplicationContext(), 0, mediaButtonIntent, 0);
    final RemoteControlClient controlClient = new RemoteControlClient(mediaPendingIntent);
    final AudioManager audioManager = (AudioManager) context
        .getSystemService(Context.AUDIO_SERVICE);
    return new RemoteControl(audioManager, controlClient, svc);
  }

  private class AudioFocus implements Releaseable {

    @Override
    public void release() {
      audioManager.abandonAudioFocus(HANDLER);
      Log.d(TAG, "Released audio focus");
    }
  }

  private Releaseable tryCaptureFocus() {
    if (AudioManager.AUDIOFOCUS_REQUEST_GRANTED == audioManager.requestAudioFocus(HANDLER,
        AudioManager.STREAM_MUSIC, AudioManager.AUDIOFOCUS_GAIN)) {
      Log.d(TAG, "Gained audio focus");
      return new AudioFocus();
    } else {
      Log.d(TAG, "Failed to gain audio focus");
      return ReleaseableStub.instance();
    }
  }

  private class StatusCallback extends CallbackStub {

    @Override
    public void onStateChanged(PlaybackControl.State state) {
      switch (state) {
        case STOPPED:
          controlClient.setPlaybackState(RemoteControlClient.PLAYSTATE_STOPPED);
          break;
        case PLAYING:
          controlClient.setPlaybackState(RemoteControlClient.PLAYSTATE_PLAYING);
          break;
        case PAUSED:
          controlClient.setPlaybackState(RemoteControlClient.PLAYSTATE_PAUSED);
          break;
      }
    }

    @Override
    public void onItemChanged(Item item) {
      try {
        final MetadataEditor meta = controlClient.editMetadata(true);
        //TODO: extract code
        final String author = item.getAuthor();
        final String title = item.getTitle();
        final boolean noAuthor = author.length() == 0;
        final boolean noTitle = title.length() == 0;
        if (noAuthor && noTitle) {
          final String filename = item.getDataId().getDisplayFilename();
          meta.putString(MediaMetadataRetriever.METADATA_KEY_TITLE, filename);
        } else {
          meta.putString(MediaMetadataRetriever.METADATA_KEY_ARTIST, author);
          meta.putString(MediaMetadataRetriever.METADATA_KEY_ALBUMARTIST, author);
          meta.putString(MediaMetadataRetriever.METADATA_KEY_TITLE, title);
        }
        meta.putLong(MediaMetadataRetriever.METADATA_KEY_DURATION,
                item.getDuration().convertTo(TimeUnit.MILLISECONDS));
        meta.apply();
      } catch (Exception e) {
        Log.w(TAG, e, "onItemChanged()");
      }
    }
  }
}
