/*
2ME — Second Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

/*
 * me-dock.cpp — Qt-Mischpult-Dock für 2ME.
 *
 * Steuert die M/E-Bänke (Quellen vom Typ "2me_bank_output") über deren
 * Proc-Handler-Schnittstelle (cut / auto_take / set_preview / set_transition /
 * set_duration / get_state). UI: Bank-Auswahl, PGM/PVW-Tally (rot/grün),
 * Preview-Bus (ein Button je Szene), Übergangstyp + Dauer, CUT / AUTO.
 *
 * Ein 200-ms-Timer pollt Bank-/Szenenliste (Auto-Refresh) und den Bank-Zustand
 * (Live-Tally). Die zuletzt gewählte Bank wird in der OBS-User-Config gemerkt.
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>
#include <util/config-file.h>

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QTimer>
#include <QString>
#include <QLayoutItem>
#include <QSignalBlocker>

#include <string.h>

#include "me-dock.h"

struct DockCtx {
	QComboBox *bankCombo = nullptr;
	QGridLayout *busGrid = nullptr;
	QLabel *pgmLabel = nullptr;
	QLabel *pvwLabel = nullptr;
	QComboBox *transCombo = nullptr;
	QSpinBox *durSpin = nullptr;
	QString bankSig;
	QString sceneSig;
	bool updatingControls = false;
};

/* ---- Persistenz: zuletzt gewählte Bank ---------------------------------- */

static QString dock_load_selected_bank()
{
	config_t *cfg = obs_frontend_get_user_config();
	if (!cfg)
		return QString();
	const char *v = config_get_string(cfg, "2ME", "selected_bank");
	return v ? QString::fromUtf8(v) : QString();
}

static void dock_save_selected_bank(const QString &name)
{
	config_t *cfg = obs_frontend_get_user_config();
	if (cfg)
		config_set_string(cfg, "2ME", "selected_bank", name.toUtf8().constData());
}

/* ---- Auf der gewählten Bank arbeiten ------------------------------------ */

static obs_source_t *dock_current_bank(QComboBox *combo)
{
	if (!combo || combo->currentIndex() < 0)
		return nullptr;
	const QString name = combo->currentData().toString();
	if (name.isEmpty())
		return nullptr;
	return obs_get_source_by_name(name.toUtf8().constData()); /* mit Ref */
}

static void dock_call(QComboBox *combo, const char *proc)
{
	obs_source_t *bank = dock_current_bank(combo);
	if (!bank)
		return;
	calldata_t cd;
	calldata_init(&cd);
	proc_handler_call(obs_source_get_proc_handler(bank), proc, &cd);
	calldata_free(&cd);
	obs_source_release(bank);
}

static void dock_proc_string(QComboBox *combo, const char *proc, const char *param, const QString &val)
{
	obs_source_t *bank = dock_current_bank(combo);
	if (!bank)
		return;
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_string(&cd, param, val.toUtf8().constData());
	proc_handler_call(obs_source_get_proc_handler(bank), proc, &cd);
	calldata_free(&cd);
	obs_source_release(bank);
}

static void dock_proc_int(QComboBox *combo, const char *proc, const char *param, long long val)
{
	obs_source_t *bank = dock_current_bank(combo);
	if (!bank)
		return;
	calldata_t cd;
	calldata_init(&cd);
	calldata_set_int(&cd, param, val);
	proc_handler_call(obs_source_get_proc_handler(bank), proc, &cd);
	calldata_free(&cd);
	obs_source_release(bank);
}

/* ---- Signaturen für Auto-Refresh ---------------------------------------- */

static QString dock_banks_signature()
{
	QString sig;
	obs_enum_sources(
		[](void *p, obs_source_t *src) -> bool {
			if (strcmp(obs_source_get_id(src), "2me_bank_output") == 0) {
				const char *n = obs_source_get_name(src);
				if (n)
					static_cast<QString *>(p)->append(QString::fromUtf8(n)).append('\n');
			}
			return true;
		},
		&sig);
	return sig;
}

static QString dock_scenes_signature()
{
	QString sig;
	obs_enum_scenes(
		[](void *p, obs_source_t *src) -> bool {
			const char *n = obs_source_get_name(src);
			if (n)
				static_cast<QString *>(p)->append(QString::fromUtf8(n)).append('\n');
			return true;
		},
		&sig);
	return sig;
}

/* ---- Befüllen --------------------------------------------------------- */

static void dock_fill_banks(DockCtx *ctx)
{
	QComboBox *combo = ctx->bankCombo;
	const QString prev = combo->currentData().toString();
	QSignalBlocker block(combo);
	combo->clear();
	obs_enum_sources(
		[](void *p, obs_source_t *src) -> bool {
			if (strcmp(obs_source_get_id(src), "2me_bank_output") == 0) {
				auto *c = static_cast<QComboBox *>(p);
				const char *n = obs_source_get_name(src);
				if (n)
					c->addItem(n, QString::fromUtf8(n));
			}
			return true;
		},
		combo);
	const QString want = prev.isEmpty() ? dock_load_selected_bank() : prev;
	const int idx = combo->findData(want);
	combo->setCurrentIndex(idx >= 0 ? idx : (combo->count() > 0 ? 0 : -1));
}

static void dock_fill_bus(DockCtx *ctx)
{
	QLayoutItem *item;
	while ((item = ctx->busGrid->takeAt(0)) != nullptr) {
		if (item->widget())
			item->widget()->deleteLater();
		delete item;
	}

	struct EnumCtx {
		DockCtx *ctx;
		int n;
	};
	EnumCtx ec{ctx, 0};

	obs_enum_scenes(
		[](void *p, obs_source_t *src) -> bool {
			auto *ec = static_cast<EnumCtx *>(p);
			const char *n = obs_source_get_name(src);
			if (!n)
				return true;
			const QString name = QString::fromUtf8(n);
			auto *btn = new QPushButton(name);
			QComboBox *combo = ec->ctx->bankCombo;
			QObject::connect(btn, &QPushButton::clicked,
					 [combo, name]() { dock_proc_string(combo, "set_preview", "scene", name); });
			ec->ctx->busGrid->addWidget(btn, ec->n / 2, ec->n % 2);
			ec->n++;
			return true;
		},
		&ec);
}

/* Übergangstyp/-dauer-Controls aus dem Bank-Zustand setzen (ohne Rückkopplung). */
static void dock_sync_controls(DockCtx *ctx)
{
	obs_source_t *bank = dock_current_bank(ctx->bankCombo);
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
	ctx->transCombo->setEnabled(bank != nullptr);
	ctx->durSpin->setEnabled(bank != nullptr);
	ctx->updatingControls = false;
}

/* ---- Aufbau + Registrierung -------------------------------------------- */

void me_dock_register(void)
{
	auto *ctx = new DockCtx();

	QWidget *root = new QWidget();
	root->setObjectName(QStringLiteral("twoMeDock"));
	QObject::connect(root, &QObject::destroyed, [ctx]() { delete ctx; });

	auto *outer = new QVBoxLayout(root);
	outer->setContentsMargins(8, 8, 8, 8);
	outer->setSpacing(6);

	/* Bank-Auswahl + Aktualisieren */
	auto *topRow = new QHBoxLayout();
	ctx->bankCombo = new QComboBox();
	auto *refresh = new QPushButton(QStringLiteral("↻"));
	refresh->setFixedWidth(32);
	refresh->setToolTip(QStringLiteral("Bänke & Szenen aktualisieren"));
	topRow->addWidget(new QLabel(QStringLiteral("Bank:")));
	topRow->addWidget(ctx->bankCombo, 1);
	topRow->addWidget(refresh);
	outer->addLayout(topRow);

	/* Tally */
	ctx->pgmLabel = new QLabel(QStringLiteral("PGM: –"));
	ctx->pvwLabel = new QLabel(QStringLiteral("PVW: –"));
	ctx->pgmLabel->setStyleSheet(QStringLiteral(
		"background:#c0392b; color:white; padding:5px; border-radius:3px; font-weight:bold;"));
	ctx->pvwLabel->setStyleSheet(QStringLiteral(
		"background:#27ae60; color:white; padding:5px; border-radius:3px; font-weight:bold;"));
	outer->addWidget(ctx->pgmLabel);
	outer->addWidget(ctx->pvwLabel);

	/* Preview-Bus */
	outer->addWidget(new QLabel(QStringLiteral("Preview-Bus (Klick = Vorschau):")));
	auto *busWidget = new QWidget();
	ctx->busGrid = new QGridLayout(busWidget);
	ctx->busGrid->setContentsMargins(0, 0, 0, 0);
	outer->addWidget(busWidget);

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
	transRow->addWidget(new QLabel(QStringLiteral("Übergang:")));
	transRow->addWidget(ctx->transCombo, 1);
	transRow->addWidget(ctx->durSpin);
	outer->addLayout(transRow);

	/* CUT / AUTO */
	auto *btnRow = new QHBoxLayout();
	auto *cutBtn = new QPushButton(QStringLiteral("CUT"));
	auto *autoBtn = new QPushButton(QStringLiteral("AUTO"));
	cutBtn->setMinimumHeight(44);
	autoBtn->setMinimumHeight(44);
	cutBtn->setStyleSheet(QStringLiteral("font-weight:bold;"));
	autoBtn->setStyleSheet(QStringLiteral("font-weight:bold; background:#e67e22; color:white;"));
	btnRow->addWidget(cutBtn);
	btnRow->addWidget(autoBtn);
	outer->addLayout(btnRow);
	outer->addStretch(1);

	/* Verdrahtung */
	QComboBox *bankCombo = ctx->bankCombo;
	QObject::connect(cutBtn, &QPushButton::clicked, [bankCombo]() { dock_call(bankCombo, "cut"); });
	QObject::connect(autoBtn, &QPushButton::clicked, [bankCombo]() { dock_call(bankCombo, "auto_take"); });
	QObject::connect(refresh, &QPushButton::clicked, [ctx]() {
		dock_fill_banks(ctx);
		dock_fill_bus(ctx);
		dock_sync_controls(ctx);
	});
	QObject::connect(ctx->bankCombo, &QComboBox::currentIndexChanged, [ctx](int) {
		dock_save_selected_bank(ctx->bankCombo->currentData().toString());
		dock_fill_bus(ctx);
		dock_sync_controls(ctx);
	});
	QObject::connect(ctx->transCombo, &QComboBox::currentIndexChanged, [ctx](int) {
		if (!ctx->updatingControls)
			dock_proc_string(ctx->bankCombo, "set_transition", "kind",
					 ctx->transCombo->currentData().toString());
	});
	QObject::connect(ctx->durSpin, &QSpinBox::valueChanged, [ctx](int v) {
		if (!ctx->updatingControls)
			dock_proc_int(ctx->bankCombo, "set_duration", "ms", v);
	});

	dock_fill_banks(ctx);
	dock_fill_bus(ctx);
	dock_sync_controls(ctx);
	ctx->bankSig = dock_banks_signature();
	ctx->sceneSig = dock_scenes_signature();

	/* Timer: Auto-Refresh + Live-Tally */
	auto *timer = new QTimer(root);
	QObject::connect(timer, &QTimer::timeout, [ctx]() {
		const QString bs = dock_banks_signature();
		if (bs != ctx->bankSig) {
			ctx->bankSig = bs;
			dock_fill_banks(ctx);
			dock_fill_bus(ctx);
			dock_sync_controls(ctx);
		}
		const QString ss = dock_scenes_signature();
		if (ss != ctx->sceneSig) {
			ctx->sceneSig = ss;
			dock_fill_bus(ctx);
		}

		obs_source_t *bank = dock_current_bank(ctx->bankCombo);
		if (!bank) {
			ctx->pgmLabel->setText(QStringLiteral("PGM: –"));
			ctx->pvwLabel->setText(QStringLiteral("PVW: –"));
			return;
		}
		calldata_t cd;
		calldata_init(&cd);
		proc_handler_call(obs_source_get_proc_handler(bank), "get_state", &cd);
		const char *pgm = calldata_string(&cd, "program");
		const char *pvw = calldata_string(&cd, "preview");
		ctx->pgmLabel->setText(QStringLiteral("PGM: ") +
				       (pgm && *pgm ? QString::fromUtf8(pgm) : QStringLiteral("–")));
		ctx->pvwLabel->setText(QStringLiteral("PVW: ") +
				       (pvw && *pvw ? QString::fromUtf8(pvw) : QStringLiteral("–")));
		calldata_free(&cd);
		obs_source_release(bank);
	});
	timer->start(200);

	const bool ok = obs_frontend_add_dock_by_id("2me_dock", "2ME", root);
	obs_log(LOG_INFO, "2ME dock registered: %s", ok ? "ok" : "FAILED");
}
