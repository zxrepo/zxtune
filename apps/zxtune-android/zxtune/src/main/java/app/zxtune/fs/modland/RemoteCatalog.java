/**
 * @file
 * @brief Remote implementation of catalog
 * @author vitamin.caig@gmail.com
 */

package app.zxtune.fs.modland;

import android.net.Uri;
import android.text.Html;

import androidx.annotation.NonNull;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.Locale;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

import app.zxtune.Log;
import app.zxtune.StubProgressCallback;
import app.zxtune.fs.ProgressCallback;
import app.zxtune.fs.api.Cdn;
import app.zxtune.fs.http.HttpObject;
import app.zxtune.fs.http.MultisourceHttpProvider;
import app.zxtune.io.Io;

/**
 * Use pure http response parsing via regex in despite that page structure seems to be xml well formed.
 *
 * There's additional api gate for xbmc plugin at
 * http://www.exotica.org.uk/mediawiki/extensions/ExoticASearch/Modland_xbmc.php
 * but it seems to be not working and has no such wide catalogue as http gate does.
 */
class RemoteCatalog extends Catalog {

  private static final String TAG = RemoteCatalog.class.getName();

  private static final String SITE = "https://www.exotica.org.uk/";
  private static final String API = SITE + "mediawiki/index.php?title=Special:Modland";
  private static final String GROUPS_QUERY = API + "&md=b_%s&st=%s";
  private static final String GROUP_TRACKS_QUERY = API + "&md=%s&id=%d";

  private static final Pattern PAGINATOR =
          Pattern.compile("<caption>Browsing (.+?) - (\\d+) results? - showing page (\\d+) of (\\d+).</caption>");
  private static final Pattern TRACKS =
          Pattern.compile("file=(pub/modules/.+?).>.+?<td class=.right.>(\\d+)</td>", Pattern.DOTALL);

  private final MultisourceHttpProvider http;
  private final Grouping authors;
  private final Grouping collections;
  private final Grouping formats;

  RemoteCatalog(MultisourceHttpProvider http) {
    this.http = http;
    this.authors = new Authors();
    this.collections = new Collections();
    this.formats = new Formats();
  }

  @Override
  public Grouping getAuthors() {
    return authors;
  }

  @Override
  public Grouping getCollections() {
    return collections;
  }

  @Override
  public Grouping getFormats() {
    return formats;
  }

  private static String decodeHtml(String txt) {
    return Html.fromHtml(txt).toString();
  }

  private static String makeGroupsQuery(String type, String filter) {
    return String.format(Locale.US, GROUPS_QUERY, type, Uri.encode(filter));
  }

  private static String makeGroupTracksQuery(String type, int authorId) {
    return String.format(Locale.US, GROUP_TRACKS_QUERY, type, authorId);
  }

  private abstract class BaseGrouping implements Grouping {

    private final Pattern entries;

    BaseGrouping() {
      this.entries = Pattern.compile(
              "<td><a href=.+?md=" + getCategoryTag() + "&amp;id=(\\d+).>(.+?)</a>.+?<td class=.right.>(\\d+)</td>", Pattern.DOTALL);
    }

    @Override
    public void queryGroups(String filter, final GroupsVisitor visitor,
                            final ProgressCallback progress) throws IOException {
      Log.d(TAG, "queryGroups(type=%s, filter=%s)", getCategoryTag(), filter);
      loadPages(makeGroupsQuery(getCategoryTag(), filter), new PagesVisitor() {

        private int total = 0;
        private int done = 0;

        @Override
        public boolean onPage(String header, int results, CharSequence content) {
          if (total == 0) {
            total = results;
            visitor.setCountHint(total);
          }
          done += parseAuthors(content, visitor);
          progress.onProgressUpdate(done, total);
          return true;
        }
      });
    }

    private int parseAuthors(CharSequence content, GroupsVisitor visitor) {
      int result = 0;
      final Matcher matcher = entries.matcher(content);
      while (matcher.find()) {
        final String id = matcher.group(1);
        final String name = decodeHtml(matcher.group(2));
        final String tracks = matcher.group(3);
        visitor.accept(new Group(Integer.valueOf(id), name, Integer.valueOf(tracks)));
        ++result;
      }
      return result;
    }

    @NonNull
    @Override
    public Group getGroup(final int id) throws IOException {
      Log.d(TAG, "getGroup(type=%s, id=%d)", getCategoryTag(), id);
      final Group[] resultRef = new Group[1];
      loadPages(makeGroupTracksQuery(getCategoryTag(), id), new PagesVisitor() {
        @Override
        public boolean onPage(String header, int results, CharSequence content) {
          final String tracksHeader = getTracksHeader();
          if (header.startsWith(tracksHeader)) {
            final String name = header.substring(tracksHeader.length());
            resultRef[0] = new Group(id, decodeHtml(name), results);
          }
          return false;
        }
      });
      final Group result = resultRef[0];
      if (result != null) {
        return result;
      }
      throw new IOException(String.format(Locale.US,"Failed to find group with id=%d", id));
    }

    @Override
    public void queryTracks(int id, final TracksVisitor visitor, final ProgressCallback progress) throws IOException {
      Log.d(TAG, "queryGroupTracks(type=%s, id=%d)", getCategoryTag(), id);
      loadPages(makeGroupTracksQuery(getCategoryTag(), id), new PagesVisitor() {

        private int total = 0;
        private int done = 0;

        @Override
        public boolean onPage(String header, int results, CharSequence content) {
          if (total == 0) {
            total = results;
            visitor.setCountHint(total);
          }
          final int tracks = parseTracks(content, visitor);
          done += tracks;
          progress.onProgressUpdate(done, total);
          return tracks != 0;
        }
      });
    }

    @NonNull
    @Override
    public Track getTrack(int id, final String filename) throws IOException {
      Log.d(TAG, "getGroupTrack(type=%s, id=%d, filename=%s)", getCategoryTag(), id, filename);
      final Track[] resultRef = {null};
      queryTracks(id, new TracksVisitor() {
        @Override
        public boolean accept(Track obj) {
          if (obj.filename.equals(filename)) {
            resultRef[0] = obj;
            return false;
          } else {
            return true;
          }
        }
      }, StubProgressCallback.instance());
      final Track result = resultRef[0];
      if (result != null) {
        return result;
      }
      throw new IOException(String.format(Locale.US, "Failed to get track '%s' with id=%d", filename, id));
    }

    abstract String getTracksHeader();

    abstract String getCategoryTag();
  }

  private class Authors extends BaseGrouping {

    @Override
    String getTracksHeader() {
      return "Modules from author ";
    }

    @Override
    String getCategoryTag() {
      return "aut";
    }
  }

  private class Collections extends BaseGrouping {

    @Override
    String getTracksHeader() {
      return "Modules from collection ";
    }

    @Override
    String getCategoryTag() {
      return "col";
    }
  }

  private class Formats extends BaseGrouping {

    @Override
    String getTracksHeader() {
      return "Modules in format ";
    }

    @Override
    String getCategoryTag() {
      return "for";
    }
  }

  private static int parseTracks(CharSequence content, TracksVisitor visitor) {
    int result = 0;
    final Matcher matcher = TRACKS.matcher(content);
    while (matcher.find()) {
      final String path = "/" + matcher.group(1);
      final String size = matcher.group(2);
      if (!visitor.accept(new Track(path, Integer.valueOf(size)))) {
        return 0;
      }
      ++result;
    }
    return result;
  }

  @Override
  @NonNull
  public ByteBuffer getTrackContent(String id) throws IOException {
    Log.d(TAG, "getTrackContent(%s)", id);
    return Io.readFrom(http.getInputStream(getContentUris(id)));
  }

  final HttpObject getTrackObject(String id) throws IOException {
    Log.d(TAG, "getTrackContent(%s)", id);
    return http.getObject(getContentUris(id));
  }

  private static Uri[] getContentUris(String id) {
    return new Uri[]{
        Cdn.modland(id),
        Uri.parse("http://ftp.amigascne.org/mirrors/ftp.modland.com" + id)
    };
  }

  interface PagesVisitor {
    boolean onPage(String title, int results, CharSequence content);
  }

  private void loadPages(String query, PagesVisitor visitor) throws IOException {
    for (int pg = 1; ; ++pg) {
      final String uri = query + String.format(Locale.US, "&pg=%d", pg);
      final String chars = Io.readHtml(http.getInputStream(Uri.parse(uri)));
      final Matcher matcher = PAGINATOR.matcher(chars);
      if (matcher.find()) {
        Log.d(TAG, "Load page: %s", matcher.group());
        final String header = matcher.group(1);
        final String results = matcher.group(2);
        final String page = matcher.group(3);
        final String pagesTotal = matcher.group(4);
        if (pg != Integer.valueOf(page)) {
          throw new IOException("Invalid paginator structure");
        }
        if (visitor.onPage(header, Integer.valueOf(results), chars)
                && pg < Integer.valueOf(pagesTotal)) {
          continue;
        }
      }
      break;
    }
  }
}
