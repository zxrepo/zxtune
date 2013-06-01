/*
 * @file
 * 
 * @brief Browser ui class
 * 
 * @version $Id:$
 * 
 * @author (C) Vitamin/CAIG
 */

package app.zxtune.ui;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.support.v4.app.Fragment;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.PopupWindow;
import app.zxtune.PlaybackService;
import app.zxtune.R;
import app.zxtune.fs.Vfs;

public class BrowserFragment extends Fragment
    implements
      BreadCrumbsUriView.OnUriSelectionListener,
      DirView.OnEntryClickListener {

  private View sources;
  private BreadCrumbsUriView position;
  private DirView listing;
  private Uri current;

  private static class Tags {

    static final String LOG = BrowserFragment.class.getName();
    static final String PATH = "path";
  }
  
  public static Fragment createInstance() {
    return new BrowserFragment();
  }
  
  public BrowserFragment() {
    setRetainInstance(true);
  }

  @Override
  public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
    return container != null ? inflater.inflate(R.layout.browser, container, false) : null;
  }

  @Override
  public void onViewCreated(View view, Bundle savedInstanceState) {
    super.onViewCreated(view, savedInstanceState);
    sources = view.findViewById(R.id.browser_sources);
    sources.setOnClickListener(new SourcesClickListener());
    position = (BreadCrumbsUriView) view.findViewById(R.id.browser_breadcrumb);
    position.setOnUriSelectionListener(this);
    listing = (DirView) view.findViewById(R.id.browser_content);
    listing.setOnEntryClickListener(this);
  }

  @Override
  public void onActivityCreated(Bundle savedState) {
    super.onActivityCreated(savedState);
    final Uri lastPath = savedState != null ? (Uri) savedState.getParcelable(Tags.PATH) : Uri.EMPTY;
    Log.d(Tags.LOG, "Restore position: " + lastPath);
    setCurrentPath(lastPath);
  }

  @Override
  public void onSaveInstanceState(Bundle outState) {
    super.onSaveInstanceState(outState);
    Log.d(Tags.LOG, "Store position: " + current);
    outState.putParcelable(Tags.PATH, current);
  }

  @Override
  public void onUriSelection(Uri uri) {
    setCurrentPath(uri);
  }

  @Override
  public void onFileClick(Uri uri) {
    final Context context = getActivity();
    final Intent intent = new Intent(Intent.ACTION_VIEW, uri, context, PlaybackService.class);
    context.startService(intent);
  }

  @Override
  public void onDirClick(Uri uri) {
    setCurrentPath(uri);
  }

  @Override
  public boolean onFileLongClick(Uri uri) {
    //TODO
    final Context context = getActivity();
    final Intent intent = new Intent(Intent.ACTION_INSERT, uri, context, PlaybackService.class);
    context.startService(intent);
    return true;
  }

  @Override
  public boolean onDirLongClick(Uri uri) {
    return false;
  }

  private final void setCurrentPath(Uri uri) {
    Log.d(Tags.LOG, "Set current path to " + uri);
    current = uri;
    position.setUri(uri);
    listing.setUri(uri);
  }

  private class SourcesClickListener extends DirView.StubOnEntryClickListener
      implements
        View.OnClickListener {

    PopupWindow popup;

    @Override
    public void onClick(View v) {
      final Context context = v.getContext();
      final DirView view = new DirView(context);
      final View root = View.inflate(context, R.layout.popup, null);
      final ViewGroup rootLayout = (ViewGroup) root.findViewById(R.id.popup_layout);
      rootLayout.addView(view);
      view.setDir(Vfs.getRoot());
      view.setOnEntryClickListener(this);

      popup =
          new PopupWindow(root, WindowManager.LayoutParams.WRAP_CONTENT,
              WindowManager.LayoutParams.WRAP_CONTENT, true);
      popup.setBackgroundDrawable(new BitmapDrawable());
      popup.setTouchable(true);
      popup.setOutsideTouchable(true);
      popup.showAsDropDown(v);
    }

    public void onDirClick(Uri uri) {
      popup.dismiss();
      BrowserFragment.this.onDirClick(uri);
    }
  }
}