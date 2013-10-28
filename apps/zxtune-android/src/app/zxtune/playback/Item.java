/**
 * @file
 * @brief Playback item interface
 * @version $Id:$
 * @author (C) Vitamin/CAIG
 */
package app.zxtune.playback;

import android.net.Uri;
import app.zxtune.TimeStamp;

/**
 *  Interface for playback item abstraction
 */
public interface Item {

  /**
   * @return Unique item identifier - may be equal to dataId in case of plain file playback
   */
  public Uri getId();

  /**
   * @return Item's data identifier
   */
  public Uri getDataId();

  /**
   * @return Item's title
   * @note may be empty
   */
  public String getTitle();

  /**
   * @return Item's author
   * @note may be empty
   */
  public String getAuthor();
  
  /**
   * @return Item's program information
   * @note may be empty
   */
  public String getProgram();
  
  /**
   * @return Item's comment
   * @note may be empty
   */
  public String getComment();

  /**
   * @return Item's duration
   */
  public TimeStamp getDuration();
}
