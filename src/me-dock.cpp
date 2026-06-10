/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

/*
 * me-dock.cpp — Qt-Mischpult-Docks für Multi-M/E (ein Dock pro Bank).
 *
 * Ein Manager (Reconcile-Timer + Frontend-Events) legt für jede Quelle vom Typ
 * "multi_me_bank" ein eigenes Dock an (Titel = Quellname, stabile ID via
 * Quell-UUID) und entfernt es wieder, sobald die Quelle verschwindet. So lassen
 * sich mehrere M/E-Bänke als getrennte, frei anordenbare Docks stapeln.
 *
 * Jedes Dock steuert genau seine Bank über deren Proc-Handler:
 *   - Preview-Bus (Szenen-Buttons, aktive = grün), CUT / AUTO
 *   - Übergangstyp + Dauer; Program/Preview als Tally-Anzeige
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QTimer>
#include <QString>
#include <QHash>
#include <QSet>
#include <QList>
#include <QDockWidget>

#include <string.h>

#include "me-dock.h"
#include "me-scenes.h"

static const char *BUS_DEFAULT = "padding:3px;";
static const char *BUS_PVW = "padding:3px; background:#27ae60; color:white; font-weight:bold;";

/* ---- FlowLayout: Buttons brechen je nach Breite automatisch um ---------- */
class FlowLayout : public QLayout {
public:
	explicit FlowLayout(QWidget *parent, int margin = 0, int spacing = 4) : QLayout(parent), m_space(spacing)
	{
		setContentsMargins(margin, margin, margin, margin);
	}
	~FlowLayout() override
	{
		QLayoutItem *it;
		while ((it = takeAt(0)))
			delete it;
	}
	void addItem(QLayoutItem *item) override { m_items.append(item); }
	int count() const override { return m_items.size(); }
	QLayoutItem *itemAt(int i) const override { return m_items.value(i); }
	QLayoutItem *takeAt(int i) override { return (i >= 0 && i < m_items.size()) ? m_items.takeAt(i) : nullptr; }
	Qt::Orientations expandingDirections() const override { return {}; }
	bool hasHeightForWidth() const override { return true; }
	int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }
	void setGeometry(const QRect &rect) override
	{
		QLayout::setGeometry(rect);
		doLayout(rect, false);
	}
	QSize sizeHint() const override { return minimumSize(); }
	QSize minimumSize() const override
	{
		QSize s;
		for (QLayoutItem *item : m_items)
			s = s.expandedTo(item->minimumSize());
		const QMargins m = contentsMargins();
		return s + QSize(m.left() + m.right(), m.top() + m.bottom());
	}

private:
	int doLayout(const QRect &rect, bool testOnly) const
	{
		const QMargins m = contentsMargins();
		const QRect eff = rect.adjusted(m.left(), m.top(), -m.right(), -m.bottom());
		int x = eff.x(), y = eff.y(), lineHeight = 0;
		for (QLayoutItem *item : m_items) {
			const QSize sz = item->sizeHint();
			int nextX = x + sz.width() + m_space;
			if (nextX - m_space > eff.right() && lineHeight > 0) {
				x = eff.x();
				y += lineHeight + m_space;
				nextX = x + sz.width() + m_space;
				lineHeight = 0;
			}
			if (!testOnly)
				item->setGeometry(QRect(QPoint(x, y), sz));
			x = nextX;
			lineHeight = qMax(lineHeight, sz.height());
		}
		return y + lineHeight - rect.y() + m.bottom();
	}
	QList<QLayoutItem *> m_items;
	int m_space;
};

/* ---- Pro-Bank-Dock-Zustand --------------------------------------------- */
struct DockCtx {
	QString uuid; /* feste Bank dieses Docks */
	QString name;
	QWidget *root = nullptr;
	FlowLayout *pvwFlow = nullptr;
	QHash<QString, QPushButton *> pvwButtons;
	QLabel *pgmLabel = nullptr;
	QLabel *pvwLabel = nullptr;
	QComboBox *transCombo = nullptr;
	QSpinBox *durSpin = nullptr;
	QString sceneSig;
	QString lastPvw;
	bool updatingControls = false;
};

static QHash<QString, DockCtx *> g_docks; /* uuid -> Dock (nur UI-Thread) */
static QTimer *g_reconcileTimer = nullptr;

static QString dock_id_for(const QString &uuid)
{
	return QStringLiteral("multime_") + uuid;
}

/* ---- Auf der Bank dieses Docks arbeiten --------------------------------- */

static obs_source_t *dock_bank(DockCtx *ctx)
{
	if (!ctx || ctx->uuid.isEmpty())
		return nullptr;
	return obs_get_source_by_uuid(ctx->uuid.toUtf8().constData()); /* mit Ref */
}

static void dock_call(DockCtx *ctx, const char *proc)
{
	obs_source_t *bank = dock_bank(ctx);
	if (!bank)
		return;
	calldata_t cd;
	calldata_init(&cd);
	proc_handler_call(obs_source_get_proc_handler(bank), proc, &cd);
	calldata_free(&cd);
	obs_source_release(bank);
}

static void dock_proc_string(DockCtx *ctx, const char *proc, const char *param, const QString &val)
{
	obs_source_t *bank = dock_bank(ctx);
	if (!bank)
		return;
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, param, val.toUtf8().constData());
	proc_handler_call(obs_source_get_proc_handler(bank), proc, &cd);
	calldata_free(&cd);
	obs_source_release(bank);
}

static void dock_proc_int(DockCtx *ctx, const char *proc, const char *param, long long val)
{
	obs_source_t *bank = dock_bank(ctx);
	if (!bank)
		return;
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_int(&cd, param, val);
	proc_handler_call(obs_source_get_proc_handler(bank), proc, &cd);
	calldata_free(&cd);
	obs_source_release(bank);
}

static QString dock_scenes_signature(const QString &uuid)
{
	QString sig;
	me_scenes_enum(
		uuid.toUtf8().constData(),
		[](void *p, const char *name, obs_source_t *) -> bool {
			static_cast<QString *>(p)->append(QString::fromUtf8(name)).append('\n');
			return true;
		},
		&sig);
	return sig;
}

/* ---- Befüllen ----------------------------------------------------------- */

static void dock_fill_bus(DockCtx *ctx)
{
	QLayoutItem *item;
	while ((item = ctx->pvwFlow->takeAt(0)) != nullptr) {
		if (item->widget())
			item->widget()->deleteLater();
		delete item;
	}
	ctx->pvwButtons.clear();
	ctx->lastPvw.clear();

	me_scenes_enum(
		ctx->uuid.toUtf8().constData(),
		[](void *p, const char *name, obs_source_t *) -> bool {
			auto *ctx = static_cast<DockCtx *>(p);
			const QString full = QString::fromUtf8(name);
			const QString shown = full.size() > 12 ? full.left(11) + QStringLiteral("…") : full;
			auto *btn = new QPushButton(shown);
			btn->setToolTip(full);
			btn->setStyleSheet(BUS_DEFAULT);
			btn->setFixedSize(86, 26);
			QObject::connect(btn, &QPushButton::clicked,
					 [ctx, full]() { dock_proc_string(ctx, "set_preview", "scene", full); });
			ctx->pvwFlow->addWidget(btn);
			ctx->pvwButtons[full] = btn;
			return true;
		},
		ctx);
}

static void dock_sync_controls(DockCtx *ctx)
{
	obs_source_t *bank = dock_bank(ctx);
	ctx->updatingControls = true;
	if (bank) {
		calldata_t cd;
		calldata_init(&cd);
		proc_handler_call(obs_source_get_proc_handler(bank), "get_state", &cd);
		const char *kind = calldata_string(&cd, "kind");
		const long long dur = calldata_int(&cd, "duration");
		const int ki = ctx->transCombo->findData(QString::fromUtf8(kind ? kind : "fade_transition"));
		if (ki >= 0)
			ctx->transCombo->setCurrentIndex(ki);
		ctx->durSpin->setValue((int)dur);
		calldata_free(&cd);
		obs_source_release(bank);
	}
	ctx->updatingControls = false;
}

static void dock_tick(DockCtx *ctx)
{
	const QString ss = dock_scenes_signature(ctx->uuid);
	if (ss != ctx->sceneSig) {
		ctx->sceneSig = ss;
		dock_fill_bus(ctx);
	}

	QString pgmN, pvwN;
	obs_source_t *bank = dock_bank(ctx);
	if (bank) {
		calldata_t cd;
		calldata_init(&cd);
		proc_handler_call(obs_source_get_proc_handler(bank), "get_state", &cd);
		const char *pgm = calldata_string(&cd, "program");
		const char *pvw = calldata_string(&cd, "preview");
		if (pgm && *pgm)
			pgmN = QString::fromUtf8(pgm);
		if (pvw && *pvw)
			pvwN = QString::fromUtf8(pvw);
		calldata_free(&cd);
		obs_source_release(bank);
	}

	ctx->pgmLabel->setText(QStringLiteral("PGM: ") + (pgmN.isEmpty() ? QStringLiteral("–") : pgmN));
	ctx->pvwLabel->setText(QStringLiteral("PVW: ") + (pvwN.isEmpty() ? QStringLiteral("–") : pvwN));

	if (pvwN != ctx->lastPvw) {
		for (QPushButton *b : ctx->pvwButtons)
			b->setStyleSheet(BUS_DEFAULT);
		if (QPushButton *b = ctx->pvwButtons.value(pvwN, nullptr))
			b->setStyleSheet(BUS_PVW);
		ctx->lastPvw = pvwN;
	}
}

/* ---- Ein Dock für eine Bank bauen + registrieren ------------------------ */

static DockCtx *build_bank_dock(const QString &uuid, const QString &name)
{
	auto *ctx = new DockCtx();
	ctx->uuid = uuid;
	ctx->name = name;

	QWidget *root = new QWidget();
	ctx->root = root;
	root->setObjectName(QStringLiteral("multiMeDock_") + uuid);
	root->setMinimumWidth(140);
	QObject::connect(root, &QObject::destroyed, [ctx]() { delete ctx; });

	auto *outer = new QVBoxLayout(root);
	outer->setContentsMargins(6, 6, 6, 6);
	outer->setSpacing(5);

	/* Tally */
	ctx->pgmLabel = new QLabel(QStringLiteral("PGM: –"));
	ctx->pgmLabel->setStyleSheet(QStringLiteral(
		"background:#c0392b; color:white; padding:3px; border-radius:3px; font-weight:bold;"));
	ctx->pvwLabel = new QLabel(QStringLiteral("PVW: –"));
	ctx->pvwLabel->setStyleSheet(QStringLiteral(
		"background:#27ae60; color:white; padding:3px; border-radius:3px; font-weight:bold;"));
	outer->addWidget(ctx->pgmLabel);
	outer->addWidget(ctx->pvwLabel);

	/* Preview-Bus in scrollbarer Fläche */
	auto *scroll = new QScrollArea();
	scroll->setWidgetResizable(true);
	scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	scroll->setMinimumHeight(40);
	auto *busWidget = new QWidget();
	ctx->pvwFlow = new FlowLayout(busWidget, 0, 4);
	scroll->setWidget(busWidget);
	outer->addWidget(scroll, 1);

	/* Übergang: Typ + Dauer */
	auto *transRow = new QHBoxLayout();
	ctx->transCombo = new QComboBox();
	ctx->transCombo->addItem(QStringLiteral("Fade"), QStringLiteral("fade_transition"));
	ctx->transCombo->addItem(QStringLiteral("Swipe"), QStringLiteral("swipe_transition"));
	ctx->transCombo->addItem(QStringLiteral("Slide"), QStringLiteral("slide_transition"));
	ctx->durSpin = new QSpinBox();
	ctx->durSpin->setRange(0, 10000);
	ctx->durSpin->setSingleStep(50);
	ctx->durSpin->setSuffix(QStringLiteral(" ms"));
	ctx->durSpin->setMaximumWidth(96);
	transRow->addWidget(ctx->transCombo, 1);
	transRow->addWidget(ctx->durSpin);
	outer->addLayout(transRow);

	/* CUT / AUTO */
	auto *btnRow = new QHBoxLayout();
	auto *cutBtn = new QPushButton(QStringLiteral("CUT"));
	auto *autoBtn = new QPushButton(QStringLiteral("AUTO"));
	cutBtn->setMinimumHeight(40);
	autoBtn->setMinimumHeight(40);
	cutBtn->setStyleSheet(QStringLiteral("font-weight:bold;"));
	autoBtn->setStyleSheet(QStringLiteral("font-weight:bold; background:#e67e22; color:white;"));
	btnRow->addWidget(cutBtn);
	btnRow->addWidget(autoBtn);
	outer->addLayout(btnRow);

	/* Verdrahtung */
	QObject::connect(cutBtn, &QPushButton::clicked, [ctx]() { dock_call(ctx, "cut"); });
	QObject::connect(autoBtn, &QPushButton::clicked, [ctx]() { dock_call(ctx, "auto_take"); });
	QObject::connect(ctx->transCombo, &QComboBox::currentIndexChanged, [ctx](int) {
		if (!ctx->updatingControls)
			dock_proc_string(ctx, "set_transition", "kind", ctx->transCombo->currentData().toString());
	});
	QObject::connect(ctx->durSpin, &QSpinBox::valueChanged, [ctx](int v) {
		if (!ctx->updatingControls)
			dock_proc_int(ctx, "set_duration", "ms", v);
	});

	dock_fill_bus(ctx);
	dock_sync_controls(ctx);
	ctx->sceneSig = dock_scenes_signature(uuid);

	auto *timer = new QTimer(root);
	QObject::connect(timer, &QTimer::timeout, [ctx]() { dock_tick(ctx); });
	timer->start(200);

	obs_frontend_add_dock_by_id(dock_id_for(uuid).toUtf8().constData(), name.toUtf8().constData(), root);
	return ctx;
}

static void update_dock_title(DockCtx *ctx, const QString &name)
{
	QWidget *w = ctx->root;
	while (w && !qobject_cast<QDockWidget *>(w))
		w = w->parentWidget();
	if (auto *dw = qobject_cast<QDockWidget *>(w))
		dw->setWindowTitle(name);
}

/* ---- Reconcile: Docks an vorhandene Bänke angleichen -------------------- */

struct BankInfo {
	QString uuid;
	QString name;
};

static void reconcile_docks()
{
	QList<BankInfo> banks;
	obs_enum_sources(
		[](void *p, obs_source_t *src) -> bool {
			if (strcmp(obs_source_get_id(src), "multi_me_bank") == 0) {
				const char *u = obs_source_get_uuid(src);
				const char *n = obs_source_get_name(src);
				if (u)
					static_cast<QList<BankInfo> *>(p)->append(
						{QString::fromUtf8(u), QString::fromUtf8(n ? n : "")});
			}
			return true;
		},
		&banks);

	QSet<QString> present;
	for (const BankInfo &b : banks) {
		present.insert(b.uuid);
		auto it = g_docks.find(b.uuid);
		if (it == g_docks.end()) {
			g_docks.insert(b.uuid, build_bank_dock(b.uuid, b.name));
		} else if (it.value()->name != b.name) {
			it.value()->name = b.name;
			update_dock_title(it.value(), b.name);
		}
	}

	for (auto it = g_docks.begin(); it != g_docks.end();) {
		if (!present.contains(it.key())) {
			const QString uuid = it.key();
			it = g_docks.erase(it);
			obs_frontend_remove_dock(dock_id_for(uuid).toUtf8().constData());
		} else {
			++it;
		}
	}
}

/* ---- Frontend-Events + Registrierung ------------------------------------ */

static void me_dock_on_frontend_event(enum obs_frontend_event event, void *data)
{
	UNUSED_PARAMETER(data);
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		reconcile_docks();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		if (g_reconcileTimer)
			g_reconcileTimer->stop();
		for (auto it = g_docks.begin(); it != g_docks.end(); ++it)
			obs_frontend_remove_dock(dock_id_for(it.key()).toUtf8().constData());
		g_docks.clear();
		obs_frontend_remove_event_callback(me_dock_on_frontend_event, NULL);
		break;
	default:
		break;
	}
}

void me_dock_register(void)
{
	obs_frontend_add_event_callback(me_dock_on_frontend_event, nullptr);

	auto *mainwin = static_cast<QWidget *>(obs_frontend_get_main_window());
	g_reconcileTimer = new QTimer(mainwin);
	QObject::connect(g_reconcileTimer, &QTimer::timeout, []() { reconcile_docks(); });
	g_reconcileTimer->start(1000);

	reconcile_docks();
	obs_log(LOG_INFO, "Multi-M/E dock manager registered");
}
