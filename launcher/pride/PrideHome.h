// SPDX-License-Identifier: GPL-3.0-only
// Pride Launcher's home window: one modpack, one big Play button.
#pragma once

#include <QPixmap>
#include <QWidget>

#include "pride/PackUpdater.h"

class QLabel;
class QProgressBar;
class QPushButton;
class BaseInstance;

namespace Pride {

/**
 * Her ask (2026-10-01): "modify a prism and put a pretty background on it that matches my mod pack with my instance
 * on it and just lock it to my modpack … build some sort of updater in it that can watch the website".
 * The window shows the dusk-world art with the PRIDE block wordmark, the signed-in account, the installed pack
 * version and whether an update is out, and one big button that does the right thing: Install → Update → Play.
 * "Advanced" opens the classic Prism window for people who want it.
 */
class PrideHome : public QWidget {
    Q_OBJECT
   public:
    explicit PrideHome(QWidget* parent = nullptr);

    static BaseInstance* prideInstance();

   signals:
    void isClosing();

   protected:
    void paintEvent(QPaintEvent*) override;
    void closeEvent(QCloseEvent*) override;
    void showEvent(QShowEvent*) override;

   private:
    void refresh();
    void onMainButton();
    void install();
    void update();
    void setBusy(bool busy, const QString& text = {});

    PackUpdater* m_updater;
    QPixmap m_hero;
    QLabel* m_account;
    QLabel* m_version;
    QLabel* m_status;
    QLabel* m_notes;
    QProgressBar* m_bar;
    QPushButton* m_main;
    bool m_checked = false;
    bool m_online = false;
    bool m_busy = false;
};

}  // namespace Pride
