/*
Abstract:
  Preferences dialog implementation

Last changed:
  $Id$

Author:
  (C) Vitamin/CAIG/2001

  This file is a part of zxtune-qt application based on zxtune library
*/

//local includes
#include "setup_preferences.h"
#include "aym.h"
#include "z80.h"
#include "sound.h"
#include "mixing.h"
#include "plugins.h"
//common includes
#include <tools.h>
//std includes
#include <algorithm>
//boost includes
#include <boost/bind.hpp>
//qt includes
#include <QtGui/QDialogButtonBox>
#include <QtGui/QTabWidget>
#include <QtGui/QVBoxLayout>

namespace
{
  class PreferencesDialog : public QDialog
  {
  public:
    PreferencesDialog(QWidget& parent, bool playing)
      : QDialog(&parent)
    {
      QDialogButtonBox* const buttons = new QDialogButtonBox(QDialogButtonBox::Ok, Qt::Horizontal, this);
      this->connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));
      QVBoxLayout* const layout = new QVBoxLayout(this);
      QTabWidget* const tabs = new QTabWidget(this);
      layout->addWidget(tabs);
      layout->addWidget(buttons);
      //fill
      QWidget* const pages[] =
      {
        UI::AYMSettingsWidget::Create(*tabs),
        UI::Z80SettingsWidget::Create(*tabs),
        UI::SoundSettingsWidget::Create(*tabs, playing),
        UI::MixingSettingsWidget::Create(*tabs, 3),
        UI::MixingSettingsWidget::Create(*tabs, 4),
        UI::PluginsSettingsWidget::Create(*tabs)
      };
      std::for_each(pages, ArrayEnd(pages),
        boost::bind(&QTabWidget::addTab, tabs, _1, boost::bind(&QWidget::windowTitle, _1)));
      setWindowTitle(tr("Preferences"));
    }
  };
}

namespace UI
{
  void ShowPreferencesDialog(QWidget& parent, bool playing)
  {
    PreferencesDialog dialog(parent, playing);
    dialog.exec();
  }
}
