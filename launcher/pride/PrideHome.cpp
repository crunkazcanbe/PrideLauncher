// SPDX-License-Identifier: GPL-3.0-only
#include "PrideHome.h"

#include <QCloseEvent>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

#include "Application.h"
#include "BaseInstance.h"
#include "InstanceImportTask.h"
#include "InstanceList.h"
#include "minecraft/auth/AccountList.h"
#include "minecraft/auth/MinecraftAccount.h"
#include "ui/dialogs/ProgressDialog.h"

namespace Pride {

static const QColor DUSK("#1e1236"), PLUM("#2c1a4f"), BLUE("#5bcefa"), PINK("#f5a9b8"), SNOW("#fff7fb"), GOLD("#ffc76b");

BaseInstance* PrideHome::prideInstance()
{
    auto* list = APPLICATION->instances();
    for (int i = 0; i < list->count(); i++)
        if (list->at(i)->name().compare("Pride", Qt::CaseInsensitive) == 0)
            return list->at(i);
    return nullptr;
}

static QPushButton* quiet(const QString& text, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setCursor(Qt::PointingHandCursor);
    b->setStyleSheet(
        "QPushButton{font-family:'Noto Sans';background:rgba(44,26,79,200);color:#fff7fb;border:1px solid #4a3378;border-radius:9px;padding:7px 14px;font-weight:600}"
        "QPushButton:hover{background:#3a2466;border-color:#5bcefa}");
    return b;
}

PrideHome::PrideHome(QWidget* parent) : QWidget(parent), m_updater(new PackUpdater(this))
{
    setWindowTitle("Pride");
    setWindowIcon(QIcon(":/pride/pride-icon.png"));
    setMinimumSize(900, 560);
    resize(1100, 640);
    m_hero = QPixmap(":/pride/hero.png");
    QFont ui("Noto Sans");                       // a friendly sans for the whole window, whatever the desktop uses
    ui.setStyleHint(QFont::SansSerif);
    ui.setPointSizeF(font().pointSizeF() > 0 ? font().pointSizeF() : 10);
    setFont(ui);

    // top-right: small doors
    auto* top = new QHBoxLayout;
    top->addStretch();
    auto* site = quiet(tr("Website"), this);
    auto* settings = quiet(tr("Game settings"), this);
    auto* accounts = quiet(tr("Accounts"), this);
    auto* advanced = quiet(tr("Advanced"), this);
    advanced->setToolTip(tr("The full Prism-style launcher window"));
    for (auto* b : { site, settings, accounts, advanced })
        top->addWidget(b);
    connect(site, &QPushButton::clicked, this, [] { QDesktopServices::openUrl(QUrl("https://pride.example.com")); });
    connect(accounts, &QPushButton::clicked, this, [this] { APPLICATION->ShowGlobalSettings(this, "accounts"); refresh(); });
    connect(settings, &QPushButton::clicked, this, [] { if (auto* i = prideInstance()) APPLICATION->showInstanceWindow(i); });
    connect(advanced, &QPushButton::clicked, this, [] { APPLICATION->showMainWindow(); });

    // bottom panel
    auto* panel = new QWidget(this);
    panel->setObjectName("panel");
    panel->setStyleSheet("#panel{background:rgba(30,18,54,220);border-top:1px solid #4a3378}");
    auto* row = new QHBoxLayout(panel);
    row->setContentsMargins(28, 18, 28, 18);
    row->setSpacing(24);

    auto* who = new QVBoxLayout;
    m_account = new QLabel(panel);
    m_account->setStyleSheet("color:#fff7fb;font-size:16px;font-weight:700");
    auto* acctHint = new QLabel(tr("Playing as"), panel);
    acctHint->setStyleSheet("color:#c9bde3");
    who->addWidget(acctHint);
    who->addWidget(m_account);
    row->addLayout(who);

    auto* mid = new QVBoxLayout;
    m_version = new QLabel(panel);
    m_version->setStyleSheet("color:#fff7fb;font-size:16px;font-weight:700");
    m_status = new QLabel(panel);
    m_status->setStyleSheet("color:#c9bde3");
    m_status->setWordWrap(true);
    m_notes = new QLabel(panel);
    m_notes->setStyleSheet("color:#c9bde3;font-size:12px");
    m_notes->setWordWrap(true);
    m_bar = new QProgressBar(panel);
    m_bar->setTextVisible(false);
    m_bar->setFixedHeight(8);
    m_bar->setStyleSheet("QProgressBar{background:#160d29;border:none;border-radius:4px}"
                         "QProgressBar::chunk{border-radius:4px;background:qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 #5bcefa,stop:1 #f5a9b8)}");
    m_bar->hide();
    mid->addWidget(m_version);
    mid->addWidget(m_status);
    mid->addWidget(m_bar);
    mid->addWidget(m_notes);
    row->addLayout(mid, 1);

    m_main = new QPushButton(panel);
    m_main->setCursor(Qt::PointingHandCursor);
    m_main->setMinimumSize(220, 64);
    m_main->setStyleSheet(
        "QPushButton{color:#1e1236;font-size:22px;font-weight:800;border:none;border-radius:14px;"
        "background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #5bcefa,stop:1 #8fdcfb)}"
        "QPushButton:hover{background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #7dd9fb,stop:1 #f5a9b8)}"
        "QPushButton:disabled{background:#4a3378;color:#c9bde3}");
    connect(m_main, &QPushButton::clicked, this, &PrideHome::onMainButton);
    row->addWidget(m_main);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* topWrap = new QWidget(this);
    topWrap->setLayout(top);
    top->setContentsMargins(20, 16, 20, 0);
    root->addWidget(topWrap);
    root->addStretch();
    root->addWidget(panel);

    connect(m_updater, &PackUpdater::checked, this, [this](bool ok, QString err) {
        m_checked = true;
        m_online = ok;
        if (!ok)
            m_status->setText(err);
        refresh();
        // an update is out and nothing is running → install it straight away (her "it tells them" flow, minus the restart)
        auto* inst = prideInstance();
        if (ok && inst && !inst->isRunning() &&
            PackUpdater::compareVersions(m_updater->manifest().version, PackUpdater::installedVersion(inst)) > 0)
            update();
    });
    connect(m_updater, &PackUpdater::progress, this, [this](qint64 d, qint64 t) {
        m_bar->setMaximum(t > 0 ? 1000 : 0);
        m_bar->setValue(t > 0 ? static_cast<int>(d * 1000 / t) : 0);
    });
    connect(m_updater, &PackUpdater::status, m_status, &QLabel::setText);
    connect(m_updater, &PackUpdater::finished, this, [this](bool ok, QString msg) {
        setBusy(false);
        m_status->setText(msg);
        if (ok)
            refresh();
    });
    refresh();
}

void PrideHome::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    if (!m_checked && !m_busy) {
        m_status->setText(tr("Checking the Pride website for updates…"));
        m_updater->check();
    }
}

void PrideHome::refresh()
{
    auto acc = APPLICATION->accounts()->defaultAccount();
    m_account->setText(acc ? acc->profileName() : tr("Nobody yet — add an account"));
    auto* inst = prideInstance();
    QString have = PackUpdater::installedVersion(inst);
    const auto& m = m_updater->manifest();
    if (!inst) {
        m_version->setText(tr("Pride isn't installed yet"));
        if (m_online && !m_busy)
            m_status->setText(tr("Pride %1 is ready to download. Press Install to get everything set up.").arg(m.version));
        m_main->setText(m_online ? tr("Install Pride") : tr("Retry"));
        m_main->setEnabled(m_checked && !m_busy);
    } else if (m_online && PackUpdater::compareVersions(m.version, have) > 0) {
        m_version->setText(have.isEmpty() ? tr("Pride (version unknown)") : tr("Pride %1").arg(have));
        m_status->setText(tr("Pride %1 is out — the launcher can update it now.").arg(m.version));
        m_main->setText(tr("Update"));
        m_main->setEnabled(!m_busy && !inst->isRunning());
    } else {
        m_version->setText(have.isEmpty() ? tr("Pride") : tr("Pride %1").arg(have));
        if (m_online)
            m_status->setText(tr("Up to date."));
        m_main->setText(inst->isRunning() ? tr("Playing…") : tr("Play"));
        m_main->setEnabled(!m_busy && !inst->isRunning());
    }
    m_notes->setText(m_online && !m.notes.isEmpty() ? tr("What's new: %1").arg(m.notes) : QString());
}

void PrideHome::setBusy(bool busy, const QString& text)
{
    m_busy = busy;
    m_bar->setVisible(busy);
    if (!text.isEmpty())
        m_status->setText(text);
    refresh();
}

void PrideHome::onMainButton()
{
    auto* inst = prideInstance();
    if (!m_online && !inst) {
        m_checked = false;
        m_updater->check();
        return;
    }
    if (!inst) {
        install();
        return;
    }
    if (m_online && PackUpdater::compareVersions(m_updater->manifest().version, PackUpdater::installedVersion(inst)) > 0) {
        update();
        return;
    }
    if (!APPLICATION->accounts()->defaultAccount()) {
        APPLICATION->ShowGlobalSettings(this, "accounts");
        refresh();
        if (!APPLICATION->accounts()->defaultAccount())
            return;
    }
    APPLICATION->launch(inst);
    refresh();
}

void PrideHome::install()
{
    const auto& m = m_updater->manifest();
    if (m.instanceZip.isEmpty()) {
        m_status->setText(tr("The website hasn't published the pack yet."));
        return;
    }
    setBusy(true, tr("Downloading Pride for the first time…"));
    auto* task = new InstanceImportTask(QUrl(m.instanceZip), this);
    task->setName("Pride");
    std::unique_ptr<Task> wrapped(APPLICATION->instances()->wrapInstanceTask(task));
    ProgressDialog dlg(this);
    dlg.execWithTask(wrapped.get());
    setBusy(false);
    if (auto* inst = prideInstance())
        m_updater->apply(inst, m);       // bring the fresh install exactly to the manifest (and record its version)
}

void PrideHome::update()
{
    auto* inst = prideInstance();
    if (!inst || inst->isRunning())
        return;
    setBusy(true, tr("Updating Pride to %1…").arg(m_updater->manifest().version));
    m_updater->apply(inst, m_updater->manifest());
}

void PrideHome::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), DUSK);
    if (!m_hero.isNull()) {               // cover, anchored a little below the middle so sun + hills show
        QSize s = m_hero.size().scaled(size(), Qt::KeepAspectRatioByExpanding);
        p.drawPixmap(QRect(QPoint((width() - s.width()) / 2, static_cast<int>((height() - s.height()) * 0.6)), s), m_hero);
    }
    QLinearGradient fade(0, 0, 0, height());
    fade.setColorAt(0, QColor(30, 18, 54, 90));
    fade.setColorAt(0.55, QColor(30, 18, 54, 0));
    fade.setColorAt(1, QColor(30, 18, 54, 160));
    p.fillRect(rect(), fade);

    // PRIDE in 5×7 blocks, rows in trans stripes
    static const char* G[5][7] = {
        { "####.", "#...#", "#...#", "####.", "#....", "#....", "#...." },
        { "####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#" },
        { "#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####" },
        { "####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####." },
        { "#####", "#....", "#....", "####.", "#....", "#....", "#####" },
    };
    static const QColor ROW[7] = { BLUE, BLUE, PINK, SNOW, PINK, BLUE, BLUE };
    int cell = std::max(10, std::min(22, width() / 48));
    int gap = std::max(2, cell / 9);
    int x0 = 48, y0 = 70;
    for (int l = 0; l < 5; l++) {
        for (int r = 0; r < 7; r++)
            for (int c = 0; c < 5; c++)
                if (G[l][r][c] == '#') {
                    QRectF b(x0 + c * (cell + gap), y0 + r * (cell + gap), cell, cell);
                    QPainterPath path;
                    path.addRoundedRect(b, cell / 7.0, cell / 7.0);
                    p.fillPath(path, ROW[r]);
                    p.fillRect(QRectF(b.left(), b.bottom() - cell / 6.0, b.width(), cell / 6.0), QColor(30, 18, 54, 70));
                }
        x0 += 5 * (cell + gap) + cell;
    }
    QFont f = font();
    f.setPointSizeF(f.pointSizeF() * 1.6);
    f.setBold(true);
    p.setFont(f);
    p.setPen(SNOW);
    p.drawText(QPointF(48, y0 + 7 * (cell + gap) + 40), tr("A modded Minecraft world where you get to be exactly who you are."));
}

void PrideHome::closeEvent(QCloseEvent* e)
{
    emit isClosing();
    e->accept();
}

}  // namespace Pride
