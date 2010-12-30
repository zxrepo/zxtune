/*
Abstract:
  Playlist container view implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "container_view.h"
#include "container_view.ui.h"
#include "playlist_view.h"
#include "playlist/io/export.h"
#include "playlist/supp/controller.h"
#include "playlist/supp/container.h"
#include "playlist/supp/scanner.h"
#include "ui/utils.h"
#include "ui/tools/filedialog.h"
//common includes
#include <logging.h>
//std includes
#include <cassert>
//boost includes
#include <boost/bind.hpp>
//qt includes
#include <QtCore/QUrl>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QMenu>

namespace
{
  const std::string THIS_MODULE("Playlist::UI::ContainerView");

  QString ExtractPlaylistName(const Playlist::IO::Container& container, const QString& defVal)
  {
    const Parameters::Accessor::Ptr params = container.GetProperties();
    Parameters::StringType val;
    if (params->FindStringValue(Playlist::ATTRIBUTE_NAME, val))
    {
      return ToQString(val);
    }
    return defVal;
  }

  class ContainerViewImpl : public Playlist::UI::ContainerView
                          , public Ui::PlaylistContainerView
  {
  public:
    ContainerViewImpl(QWidget& parent, Parameters::Accessor::Ptr parameters)
      : Playlist::UI::ContainerView(parent)
      , Container(Playlist::Container::Create(*this, parameters))
      , ActionsMenu(new QMenu(tr("Playlist"), this))
      , Filer(FileDialog::Create(*this))
      , ActivePlaylistView(0)
    {
      //setup self
      setupUi(this);
      setAcceptDrops(true);
      SetupMenu();

      //connect actions
      this->connect(actionAddFiles, SIGNAL(triggered()), SLOT(AddFiles()));
      this->connect(actionAddFolder, SIGNAL(triggered()), SLOT(AddFolder()));
      //playlist actions
      this->connect(actionCreatePlaylist, SIGNAL(triggered()), SLOT(CreatePlaylist()));
      this->connect(actionLoadPlaylist, SIGNAL(triggered()), SLOT(LoadPlaylist()));
      this->connect(actionSavePlaylist, SIGNAL(triggered()), SLOT(SavePlaylist()));
      this->connect(actionClosePlaylist, SIGNAL(triggered()), SLOT(CloseCurrentPlaylist()));
      this->connect(actionClearPlaylist, SIGNAL(triggered()), SLOT(Clear()));

      this->connect(widgetsContainer, SIGNAL(tabCloseRequested(int)), SLOT(ClosePlaylist(int)));

      Log::Debug(THIS_MODULE, "Created at %1%", this);
    }

    virtual ~ContainerViewImpl()
    {
      Log::Debug(THIS_MODULE, "Destroyed at %1%", this);
    }

    virtual void CreatePlaylist(const QStringList& items)
    {
      Playlist::UI::View& pl = CreateAnonymousPlaylist();
      const bool deepScan = actionDeepScan->isChecked();
      pl.AddItems(items, deepScan);
    }

    virtual QMenu* GetActionsMenu() const
    {
      return ActionsMenu;
    }

    virtual void Play()
    {
      Playlist::UI::View& pl = GetActivePlaylist();
      pl.Play();
    }

    virtual void Pause()
    {
      Playlist::UI::View& pl = GetActivePlaylist();
      pl.Pause();
    }

    virtual void Stop()
    {
      Playlist::UI::View& pl = GetActivePlaylist();
      pl.Stop();
    }

    virtual void Finish()
    {
      Playlist::UI::View& pl = GetActivePlaylist();
      pl.Finish();
    }

    virtual void Next()
    {
      Playlist::UI::View& pl = GetActivePlaylist();
      pl.Next();
    }

    virtual void Prev()
    {
      Playlist::UI::View& pl = GetActivePlaylist();
      pl.Prev();
    }

    virtual void Clear()
    {
      Playlist::UI::View& pl = GetVisiblePlaylist();
      pl.Clear();
    }

    virtual void AddFiles()
    {
      QStringList files;
      if (Filer->OpenMultipleFiles(actionAddFiles->text(),
        tr("All files (*.*)"), files))
      {
        AddItemsToVisiblePlaylist(files);
      }
    }

    virtual void AddFolder()
    {
      QStringList folders;
      folders += QString();
      if (Filer->OpenFolder(actionAddFolder->text(), folders.front()))
      {
        AddItemsToVisiblePlaylist(folders);
      }
    }

    virtual void CreatePlaylist()
    {
      CreateAnonymousPlaylist();
    }

    virtual void LoadPlaylist()
    {
      QString file;
      if (Filer->OpenSingleFile(actionLoadPlaylist->text(),
         tr("Playlist files (*.xspf *.ayl)"), file))
      {
        if (Playlist::Controller::Ptr pl = Container->OpenPlaylist(file))
        {
          RegisterPlaylist(pl);
        }
      }
    }

    virtual void SavePlaylist()
    {
      Playlist::UI::View& pl = GetVisiblePlaylist();
      const Playlist::Controller& controller = pl.GetPlaylist();
      const Playlist::IO::Container::Ptr container = controller.GetContainer();
      QString filename = ExtractPlaylistName(*container, controller.GetName());
      if (Filer->SaveFile(actionSavePlaylist->text(),
        QString::fromUtf8("xspf"),
        QString::fromUtf8("Playlist files (*.xspf)"),
        filename))
      {
        if (!Playlist::IO::SaveXSPF(container, filename))
        {
          assert(!"Failed to save");
        }
      }
    }

    virtual void CloseCurrentPlaylist()
    {
      ClosePlaylist(widgetsContainer->currentIndex());
    }

    virtual void ClosePlaylist(int index)
    {
      Playlist::UI::View* const view = static_cast<Playlist::UI::View*>(widgetsContainer->widget(index));
      widgetsContainer->removeTab(index);
      Log::Debug(THIS_MODULE, "Closed playlist idx=%1% val=%2%, active=%3%",
        index, view, ActivePlaylistView);
      if (view == ActivePlaylistView)
      {
        ActivePlaylistView = 0;
        SwitchToLastPlaylist();
      }
      view->deleteLater();
    }

    //base virtuals
    virtual void dragEnterEvent(QDragEnterEvent* event)
    {
      event->acceptProposedAction();
    }

    virtual void dropEvent(QDropEvent* event)
    {
      const QMimeData* const mimeData = event->mimeData();
      if (mimeData && mimeData->hasUrls())
      {
        const QList<QUrl>& urls = mimeData->urls();
        QStringList files;
        std::for_each(urls.begin(), urls.end(),
          boost::bind(&QStringList::push_back, &files,
            boost::bind(&QUrl::toLocalFile, _1)));
        AddItemsToVisiblePlaylist(files);
      }
    }
  private:
    void PlaylistItemActivated(const Playlist::Item::Data& item)
    {
      if (QObject* sender = this->sender())
      {
        //assert(dynamic_cast<Playlist::UI::View*>(sender));
        Playlist::UI::View* const newView = static_cast<Playlist::UI::View*>(sender);
        if (newView != ActivePlaylistView)
        {
          Log::Debug(THIS_MODULE, "Switched playlist %1% -> %2%", newView, ActivePlaylistView);
          ActivePlaylistView->Stop();
          ActivePlaylistView = newView;
        }
      }
      OnItemActivated(item);
    }
  private:
    void SetupMenu()
    {
      ActionsMenu->addAction(actionAddFiles);
      ActionsMenu->addAction(actionAddFolder);
      ActionsMenu->addAction(actionDeepScan);
      ActionsMenu->addSeparator();
      ActionsMenu->addAction(actionCreatePlaylist);
      ActionsMenu->addAction(actionLoadPlaylist);
      ActionsMenu->addAction(actionSavePlaylist);
      ActionsMenu->addAction(actionClosePlaylist);
      ActionsMenu->addAction(actionClearPlaylist);
      ActionsMenu->addSeparator();
      ActionsMenu->addAction(actionLoop);
      ActionsMenu->addAction(actionRandom);
    }

    Playlist::UI::View& CreateAnonymousPlaylist()
    {
      Log::Debug(THIS_MODULE, "Create default playlist");
      const Playlist::Controller::Ptr pl = Container->CreatePlaylist(tr("Default"));
      return RegisterPlaylist(pl);
    }

    Playlist::UI::View& RegisterPlaylist(Playlist::Controller::Ptr playlist)
    {
      Playlist::UI::View* const plView = Playlist::UI::View::Create(*this, playlist);
      widgetsContainer->addTab(plView, playlist->GetName());
      this->connect(plView, SIGNAL(OnItemActivated(const Playlist::Item::Data&)),
        SLOT(PlaylistItemActivated(const Playlist::Item::Data&)));
      plView->connect(actionLoop, SIGNAL(triggered(bool)), SLOT(SetIsLooped(bool)));
      plView->connect(actionRandom, SIGNAL(triggered(bool)), SLOT(SetIsRandomized(bool)));
      if (!ActivePlaylistView)
      {
        ActivePlaylistView = plView;
      }
      widgetsContainer->setCurrentWidget(plView);
      return *plView;
    }

    Playlist::UI::View& GetActivePlaylist()
    {
      if (!ActivePlaylistView)
      {
        return CreateAnonymousPlaylist();
      }
      return *ActivePlaylistView;
    }

    Playlist::UI::View& GetVisiblePlaylist()
    {
      if (Playlist::UI::View* view = static_cast<Playlist::UI::View*>(widgetsContainer->currentWidget()))
      {
        return *view;
      }
      return GetActivePlaylist();
    }

    void AddItemsToVisiblePlaylist(const QStringList& items)
    {
      const bool deepScan = actionDeepScan->isChecked();
      Playlist::UI::View& pl = GetVisiblePlaylist();
      pl.AddItems(items, deepScan);
    }

    void SwitchToLastPlaylist()
    {
      Log::Debug(THIS_MODULE, "Move to another playlist");
      if (int total = widgetsContainer->count())
      {
        ActivatePlaylist(total - 1);
      }
      else
      {
        CreateAnonymousPlaylist();
      }
    }

    void ActivatePlaylist(int index)
    {
      if (QWidget* widget = widgetsContainer->widget(index))
      {
        ActivePlaylistView = static_cast<Playlist::UI::View*>(widget);
        Log::Debug(THIS_MODULE, "Switching to playlist idx=%1% val=%2%", index, ActivePlaylistView);
      }
    }
  private:
    const Playlist::Container::Ptr Container;
    QMenu* const ActionsMenu;
    const FileDialog::Ptr Filer;
    //state context
    Playlist::UI::View* ActivePlaylistView;
  };
}

namespace Playlist
{
  namespace UI
  {
    ContainerView::ContainerView(QWidget& parent) : QWidget(&parent)
    {
    }

    ContainerView* ContainerView::Create(QWidget& parent, Parameters::Accessor::Ptr parameters)
    {
      return new ContainerViewImpl(parent, parameters);
    }
  }
}
