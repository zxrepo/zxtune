/**
 *
 * @file
 *
 * @brief Caching catalog implementation
 *
 * @author vitamin.caig@gmail.com
 *
 */

package app.zxtune.fs.amp;

import java.io.IOException;
import java.nio.ByteBuffer;
import java.util.concurrent.TimeUnit;

import app.zxtune.Log;
import app.zxtune.TimeStamp;
import app.zxtune.fs.VfsCache;
import app.zxtune.fs.amp.Catalog.FoundTracksVisitor;
import app.zxtune.fs.dbhelpers.QueryCommand;
import app.zxtune.fs.dbhelpers.Timestamps;
import app.zxtune.fs.dbhelpers.Transaction;
import app.zxtune.fs.dbhelpers.Utils;

final class CachingCatalog extends Catalog {

  private final static String TAG = CachingCatalog.class.getName();

  private final TimeStamp GROUPS_TTL = days(30);
  private final TimeStamp AUTHORS_TTL = days(30);
  private final TimeStamp COUNTRIES_TTL = days(30);
  private final TimeStamp TRACKS_TTL = days(30);
  
  private static TimeStamp days(int val) {
    return TimeStamp.createFrom(val, TimeUnit.DAYS);
  }
  
  private final Catalog remote;
  private final Database db;
  private final VfsCache cacheDir;

  public CachingCatalog(Catalog remote, Database db, VfsCache cacheDir) {
    this.remote = remote;
    this.db = db;
    this.cacheDir = cacheDir; 
  }

  @Override
  public void queryGroups(final GroupsVisitor visitor) throws IOException {
    Utils.executeQueryCommand(new QueryCommand() {
      @Override
      public Timestamps.Lifetime getLifetime() {
        return db.getGroupsLifetime(GROUPS_TTL);
      }

      @Override
      public Transaction startTransaction() {
        return db.startTransaction();
      }
      
      @Override
      public void queryFromRemote() throws IOException {
        Log.d(TAG, "Groups cache is empty/expired");
        remote.queryGroups(new CachingGroupsVisitor());
      }
      
      @Override
      public boolean queryFromCache() {
        return db.queryGroups(visitor);
      }
    });
  }
  
  @Override
  public void queryAuthors(final String handleFilter, final AuthorsVisitor visitor) throws IOException {
    Utils.executeQueryCommand(new QueryCommand() {
      
      @Override
      public Timestamps.Lifetime getLifetime() {
        return db.getAuthorsLifetime(handleFilter, AUTHORS_TTL);
      }

      @Override
      public Transaction startTransaction() {
        return db.startTransaction();
      }
      
      @Override
      public void queryFromRemote() throws IOException {
        Log.d(TAG, "Authors cache is empty/expired for handleFilter=%s", handleFilter);
        remote.queryAuthors(handleFilter, new CachingAuthorsVisitor());
      }
      
      @Override
      public boolean queryFromCache() {
        return db.queryAuthors(handleFilter, visitor);
      }
    });
  }

  @Override
  public void queryAuthors(final Country country, final AuthorsVisitor visitor) throws IOException {
    Utils.executeQueryCommand(new QueryCommand() {
      
      @Override
      public Timestamps.Lifetime getLifetime() {
        return db.getCountryLifetime(country, COUNTRIES_TTL);
      }

      @Override
      public Transaction startTransaction() {
        return db.startTransaction();
      }

      @Override
      public void queryFromRemote() throws IOException {
        Log.d(TAG, "Authors cache is empty/expired for country=%d", country.id);
        remote.queryAuthors(country, new CachingAuthorsVisitor(country));
      }
      
      @Override
      public boolean queryFromCache() {
        return db.queryAuthors(country, visitor);
      }
    });
  }
  
  @Override
  public void queryAuthors(final Group group, final AuthorsVisitor visitor) throws IOException {
    Utils.executeQueryCommand(new QueryCommand() {
      
      @Override
      public Timestamps.Lifetime getLifetime() {
        return db.getGroupLifetime(group, GROUPS_TTL);
      }

      @Override
      public Transaction startTransaction() {
        return db.startTransaction();
      }

      @Override
      public void queryFromRemote() throws IOException {
        Log.d(TAG, "Authors cache is empty/expired for group=%d", group.id);
        remote.queryAuthors(group, new CachingAuthorsVisitor(group));
      }
      
      @Override
      public boolean queryFromCache() {
        return db.queryAuthors(group, visitor);
      }
    });
  }
  
  @Override
  public void queryTracks(final Author author, final TracksVisitor visitor) throws IOException {
    Utils.executeQueryCommand(new QueryCommand() {

      @Override
      public Timestamps.Lifetime getLifetime() {
        return db.getAuthorTracksLifetime(author, TRACKS_TTL);
      }

      @Override
      public Transaction startTransaction() {
        return db.startTransaction();
      }

      @Override
      public boolean queryFromCache() {
        return db.queryTracks(author, visitor);
      }

      @Override
      public void queryFromRemote() throws IOException {
        Log.d(TAG, "Tracks cache is empty/expired for author=%d", author.id);
        remote.queryTracks(author, new CachingTracksVisitor(author));
      }
    });
  }
  
  @Override
  public boolean searchSupported() {
    return true;
  }
  
  @Override
  public void findTracks(String query, FoundTracksVisitor visitor) throws IOException {
    if (remote.searchSupported()) {
      Log.d(TAG, "Use remote-side search");
      remote.findTracks(query, visitor);
    } else {
      Log.d(TAG, "Use local search");
      db.findTracks(query, visitor);
    }
  }
  
  @Override
  public ByteBuffer getTrackContent(int id) throws IOException {
    final String filename = Integer.toString(id);
    final ByteBuffer cachedContent = cacheDir.getCachedFileContent(filename);
    if (cachedContent != null) {
      return cachedContent;
    } else {
      final ByteBuffer content = remote.getTrackContent(id);
      cacheDir.putCachedFileContent(filename, content);
      return content;
    }
  }
  
  private class CachingGroupsVisitor extends GroupsVisitor {
    
    @Override
    public void accept(Group obj) {
      try {
        db.addGroup(obj);
      } catch (Exception e) {
        Log.d(TAG, e, "acceptGroup()");
      }
    }
  }
  
  private class CachingAuthorsVisitor extends AuthorsVisitor {
    
    private final Country country;
    private final Group group;
    
    CachingAuthorsVisitor() {
      this.country = null;
      this.group = null;
    }
    
    CachingAuthorsVisitor(Country country) {
      this.country = country;
      this.group = null;
    }
    
    CachingAuthorsVisitor(Group group) {
      this.country = null;
      this.group = group;
    }

    @Override
    public void accept(Author obj) {
      try {
        db.addAuthor(obj);
        if (country != null) {
          db.addCountryAuthor(country, obj);
        }
        if (group != null) {
          db.addGroupAuthor(group, obj);
        }
      } catch (Exception e) {
        Log.d(TAG, e, "acceptAuthor()");
      }
    }
  }
  
  private class CachingTracksVisitor extends TracksVisitor {
    
    private final Author author;
    
    public CachingTracksVisitor(Author author) {
      this.author = author;
    }
    
    @Override
    public void accept(Track obj) {
      try {
        db.addTrack(obj);
        db.addAuthorTrack(author, obj);
      } catch (Exception e) {
        Log.d(TAG, e, "acceptTrack()");
      }
    }
  }
}