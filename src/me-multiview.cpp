/*
Multi-M/E — Multiple Mix/Effects for OBS
Copyright (C) 2026 Paul Loth <paulloth2208@gmail.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version. See <https://www.gnu.org/licenses/>.
*/

/*
 * me-multiview.cpp — Native-artige Multiview je M/E-Bank.
 *
 * Eigenständiges Projektor-Fenster (Fenster/Vollbild) wie die OBS-Multiview:
 * oben PVW (links) + PGM (rechts), darunter bis zu 8 Szenen-Thumbnails (4x2).
 * Klick = Szene in Preview, Doppelklick = AUTO-Take. Esc schließt. Farbige
 * Rahmen + Text-Labels (PREVIEW/PROGRAM + Szenennamen).
 *
 * Geöffnet über Tools → "Multi-M/E Multiview" → je Bank Fenster/Vollbild.
 */

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <graphics/vec4.h>

#include <QWidget>
#include <QMenu>
#include <QAction>
#include <QGuiApplication>
#include <QScreen>
#include <QPaintEngine>
#include <QResizeEvent>
#include <QShowEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QTimer>
#include <QRect>
#include <QVector>
#include <QString>
#include <QList>
#include <QHash>
#include <QPointer>
#include <QImage>
#include <QPainter>
#include <QFont>
#include <QFontMetrics>
#include <QColor>

#include <mutex>
#include <vector>
#include <string.h>

#include "me-multiview.h"
#include "me-scenes.h"

static const int MV_MAX_THUMBS = 8;

/* ---- Helfer: Bank-Liste + Procs ----------------------------------------- */

struct BankInfo {
	QString uuid;
	QString name;
};

static QList<BankInfo> mv_enum_banks()
{
	QList<BankInfo> list;
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
		&list);
	return list;
}

static void mv_bank_call(const QString &uuid, const char *proc, const char *param, const QString *val)
{
	obs_source_t *bank = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (!bank)
		return;
	calldata_t cd;
	calldata_init(&cd);
	if (param && val)
		calldata_set_string(&cd, param, val->toUtf8().constData());
	proc_handler_call(obs_source_get_proc_handler(bank), proc, &cd);
	calldata_free(&cd);
	obs_source_release(bank);
}

static void mv_set_preview(const QString &uuid, const QString &scene)
{
	mv_bank_call(uuid, "set_preview", "scene", &scene);
}
static void mv_auto(const QString &uuid)
{
	mv_bank_call(uuid, "auto_take", nullptr, nullptr);
}

static QImage mv_render_label(const QString &text)
{
	QFont font;
	font.setPixelSize(14);
	font.setBold(true);
	QFontMetrics fm(font);
	int tw = fm.horizontalAdvance(text) + 10;
	int th = fm.height() + 4;
	QImage img(tw, th, QImage::Format_RGBA8888_Premultiplied);
	img.fill(QColor(0, 0, 0, 150));
	QPainter p(&img);
	p.setFont(font);
	p.setPen(Qt::white);
	p.drawText(QRect(5, 2, tw - 10, th - 4), Qt::AlignLeft | Qt::AlignVCenter, text);
	p.end();
	return img;
}

/* ---- Grafik-Helfer (im Display-Render-Kontext) -------------------------- */

static void mv_draw_box(int x, int y, int w, int h, const struct vec4 &color)
{
	if (w <= 0 || h <= 0)
		return;
	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &color);
	gs_viewport_push();
	gs_projection_push();
	gs_set_viewport(x, y, w, h);
	gs_ortho(0.0f, 1.0f, 0.0f, 1.0f, -100.0f, 100.0f);
	while (gs_effect_loop(solid, "Solid"))
		gs_draw_sprite(nullptr, 0, 1, 1);
	gs_projection_pop();
	gs_viewport_pop();
}

static void mv_render_source(obs_source_t *src, int x, int y, int w, int h)
{
	if (!src || w <= 0 || h <= 0)
		return;
	uint32_t sw = obs_source_get_width(src);
	uint32_t sh = obs_source_get_height(src);
	if (!sw || !sh)
		return;
	float scale = qMin((float)w / (float)sw, (float)h / (float)sh);
	int dw = (int)(sw * scale), dh = (int)(sh * scale);
	int dx = x + (w - dw) / 2, dy = y + (h - dh) / 2;
	gs_viewport_push();
	gs_projection_push();
	gs_set_viewport(dx, dy, dw, dh);
	gs_ortho(0.0f, (float)sw, 0.0f, (float)sh, -100.0f, 100.0f);
	obs_source_video_render(src);
	gs_projection_pop();
	gs_viewport_pop();
}

static void mv_draw_texture(gs_texture_t *tex, int x, int y)
{
	if (!tex)
		return;
	int tw = (int)gs_texture_get_width(tex);
	int th = (int)gs_texture_get_height(tex);
	gs_effect_t *eff = obs_get_base_effect(OBS_EFFECT_DEFAULT);
	gs_effect_set_texture(gs_effect_get_param_by_name(eff, "image"), tex);
	gs_viewport_push();
	gs_projection_push();
	gs_set_viewport(x, y, tw, th);
	gs_ortho(0.0f, (float)tw, 0.0f, (float)th, -100.0f, 100.0f);
	gs_blend_state_push();
	gs_enable_blending(true);
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
	while (gs_effect_loop(eff, "Draw"))
		gs_draw_sprite(tex, 0, (uint32_t)tw, (uint32_t)th);
	gs_blend_state_pop();
	gs_projection_pop();
	gs_viewport_pop();
}

/* ---- Multiview-Fenster -------------------------------------------------- */

struct SceneRef {
	QString name;
	obs_weak_source_t *weak;
};

class MEMultiview : public QWidget {
public:
	MEMultiview(const QString &uuid, const QString &name);
	~MEMultiview() override;
	QPaintEngine *paintEngine() const override { return nullptr; }

protected:
	void showEvent(QShowEvent *) override { createDisplay(); }
	void resizeEvent(QResizeEvent *) override
	{
		if (m_display) {
			qreal dpr = devicePixelRatioF();
			obs_display_resize(m_display, (uint32_t)(width() * dpr), (uint32_t)(height() * dpr));
		} else {
			createDisplay();
		}
	}
	void mousePressEvent(QMouseEvent *e) override;
	void mouseDoubleClickEvent(QMouseEvent *e) override;
	void keyPressEvent(QKeyEvent *e) override
	{
		if (e->key() == Qt::Key_Escape)
			close();
		else
			QWidget::keyPressEvent(e);
	}

private:
	struct Layout {
		QRect pvw, pgm;
		QVector<QRect> thumbs;
	};
	static Layout computeLayout(int w, int h, int n);
	int thumbAt(const QPoint &pos, QString &nameOut);
	void createDisplay();
	void refreshSnapshot();
	void clearScenes(std::vector<SceneRef> &v);
	static void drawCb(void *param, uint32_t cx, uint32_t cy);
	void render(uint32_t cx, uint32_t cy);
	gs_texture_t *labelTexture(const QString &text); /* nur Grafik-Thread */

	QString m_uuid;
	obs_display_t *m_display = nullptr;
	obs_weak_source_t *m_programWeak = nullptr;
	std::mutex m_mutex;
	std::vector<SceneRef> m_scenes;
	QString m_pgmName, m_pvwName;
	QHash<QString, QImage> m_labelImages;       /* UI-Thread baut, Render liest (mutex) */
	QHash<QString, gs_texture_t *> m_textCache; /* nur Grafik-Thread */
	QString m_sceneSig;
	QTimer *m_timer = nullptr;
};

MEMultiview::MEMultiview(const QString &uuid, const QString &name) : QWidget(nullptr), m_uuid(uuid)
{
	setAttribute(Qt::WA_PaintOnScreen);
	setAttribute(Qt::WA_StaticContents);
	setAttribute(Qt::WA_NoSystemBackground);
	setAttribute(Qt::WA_OpaquePaintEvent);
	setAttribute(Qt::WA_DontCreateNativeAncestors);
	setAttribute(Qt::WA_NativeWindow);
	setAttribute(Qt::WA_DeleteOnClose);
	setWindowTitle(QStringLiteral("Multi-M/E Multiview — ") + name);
	setMinimumSize(320, 240);
	setFocusPolicy(Qt::StrongFocus);

	obs_source_t *bank = obs_get_source_by_uuid(uuid.toUtf8().constData());
	if (bank) {
		m_programWeak = obs_source_get_weak_source(bank);
		obs_source_inc_showing(bank);
		obs_source_release(bank);
	}

	refreshSnapshot();
	m_timer = new QTimer(this);
	QObject::connect(m_timer, &QTimer::timeout, [this]() { refreshSnapshot(); });
	m_timer->start(250);
}

MEMultiview::~MEMultiview()
{
	if (m_display) {
		obs_display_remove_draw_callback(m_display, drawCb, this);
		obs_display_destroy(m_display);
		m_display = nullptr;
	}
	if (!m_textCache.isEmpty()) {
		obs_enter_graphics();
		for (gs_texture_t *t : m_textCache)
			gs_texture_destroy(t);
		obs_leave_graphics();
		m_textCache.clear();
	}
	clearScenes(m_scenes);
	obs_source_t *pgm = obs_weak_source_get_source(m_programWeak);
	if (pgm) {
		obs_source_dec_showing(pgm);
		obs_source_release(pgm);
	}
	obs_weak_source_release(m_programWeak);
}

void MEMultiview::clearScenes(std::vector<SceneRef> &v)
{
	for (SceneRef &s : v) {
		obs_source_t *src = obs_weak_source_get_source(s.weak);
		if (src) {
			obs_source_dec_showing(src);
			obs_source_release(src);
		}
		obs_weak_source_release(s.weak);
	}
	v.clear();
}

void MEMultiview::createDisplay()
{
	if (m_display || width() <= 0 || height() <= 0 || !isVisible())
		return;
	qreal dpr = devicePixelRatioF();
	gs_init_data init = {};
	init.cx = (uint32_t)(width() * dpr);
	init.cy = (uint32_t)(height() * dpr);
	init.format = GS_BGRA;
	init.zsformat = GS_ZS_NONE;
	init.num_backbuffers = 1;
#ifdef _WIN32
	init.window.hwnd = (void *)winId();
#elif defined(__APPLE__)
	init.window.view = (id)(void *)winId();
#endif
	m_display = obs_display_create(&init, 0x111111);
	if (m_display)
		obs_display_add_draw_callback(m_display, drawCb, this);
}

void MEMultiview::refreshSnapshot()
{
	QString pgmN, pvwN;
	obs_source_t *bank = obs_get_source_by_uuid(m_uuid.toUtf8().constData());
	if (bank) {
		calldata_t cd;
		calldata_init(&cd);
		proc_handler_call(obs_source_get_proc_handler(bank), "get_state", &cd);
		const char *pg = calldata_string(&cd, "program");
		const char *pv = calldata_string(&cd, "preview");
		if (pg)
			pgmN = QString::fromUtf8(pg);
		if (pv)
			pvwN = QString::fromUtf8(pv);
		calldata_free(&cd);
		obs_source_release(bank);
	}

	/* Pass 1: Signatur der gefilterten Szenen (max 8), ohne inc_showing */
	struct SigCtx {
		QString sig;
		int n;
	} sc{QString(), 0};
	me_scenes_enum(
		m_uuid.toUtf8().constData(),
		[](void *p, const char *name, obs_source_t *) -> bool {
			auto *c = static_cast<SigCtx *>(p);
			if (c->n >= MV_MAX_THUMBS)
				return false;
			c->sig.append(QString::fromUtf8(name)).append('\n');
			c->n++;
			return true;
		},
		&sc);

	if (sc.sig != m_sceneSig) {
		/* Pass 2: Szenen sammeln (inc_showing) + Labels rendern (max 8) */
		struct ColCtx {
			std::vector<SceneRef> fresh;
			QHash<QString, QImage> labels;
		} cc;
		cc.labels.insert(QStringLiteral("PREVIEW"), mv_render_label(QStringLiteral("PREVIEW")));
		cc.labels.insert(QStringLiteral("PROGRAM"), mv_render_label(QStringLiteral("PROGRAM")));
		me_scenes_enum(
			m_uuid.toUtf8().constData(),
			[](void *p, const char *name, obs_source_t *scene) -> bool {
				auto *c = static_cast<ColCtx *>(p);
				if ((int)c->fresh.size() >= MV_MAX_THUMBS)
					return false;
				QString qn = QString::fromUtf8(name);
				obs_source_inc_showing(scene);
				c->fresh.push_back({qn, obs_source_get_weak_source(scene)});
				if (!c->labels.contains(qn))
					c->labels.insert(qn, mv_render_label(qn));
				return true;
			},
			&cc);

		std::vector<SceneRef> old;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			old.swap(m_scenes);
			m_scenes = std::move(cc.fresh);
			m_labelImages = cc.labels;
			m_pgmName = pgmN;
			m_pvwName = pvwN;
		}
		clearScenes(old);
		m_sceneSig = sc.sig;
	} else {
		std::lock_guard<std::mutex> lock(m_mutex);
		m_pgmName = pgmN;
		m_pvwName = pvwN;
	}
}

MEMultiview::Layout MEMultiview::computeLayout(int w, int h, int n)
{
	Layout L;
	const int gap = 4;
	n = qMin(n, MV_MAX_THUMBS);
	int topH = (n > 0) ? (int)(h * 0.5) : h;
	L.pvw = QRect(0, 0, w / 2 - gap / 2, topH - gap);
	L.pgm = QRect(w / 2 + gap / 2, 0, w - (w / 2 + gap / 2), topH - gap);

	int botY = topH;
	int botH = h - botY;
	if (n > 0 && botH > 0) {
		const int cols = 4;
		int rows = (n + cols - 1) / cols;
		int cw = w / cols;
		int ch = botH / rows;
		L.thumbs.reserve(n);
		for (int i = 0; i < n; i++) {
			int c = i % cols, r = i / cols;
			L.thumbs.append(QRect(c * cw, botY + r * ch, cw - gap, ch - gap));
		}
	}
	return L;
}

gs_texture_t *MEMultiview::labelTexture(const QString &text)
{
	auto it = m_textCache.find(text);
	if (it != m_textCache.end())
		return it.value();
	QImage img;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		auto i2 = m_labelImages.find(text);
		if (i2 == m_labelImages.end())
			return nullptr;
		img = i2.value(); /* COW-Kopie */
	}
	const uint8_t *data = img.constBits();
	gs_texture_t *tex = gs_texture_create((uint32_t)img.width(), (uint32_t)img.height(), GS_RGBA, 1, &data, 0);
	m_textCache.insert(text, tex);
	return tex;
}

void MEMultiview::drawCb(void *param, uint32_t cx, uint32_t cy)
{
	static_cast<MEMultiview *>(param)->render(cx, cy);
}

void MEMultiview::render(uint32_t cx, uint32_t cy)
{
	struct Item {
		QString name;
		obs_source_t *src;
	};
	std::vector<Item> items;
	QString pgmN, pvwN;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		pgmN = m_pgmName;
		pvwN = m_pvwName;
		items.reserve(m_scenes.size());
		for (SceneRef &s : m_scenes)
			items.push_back({s.name, obs_weak_source_get_source(s.weak)});
	}

	obs_source_t *pgm = obs_weak_source_get_source(m_programWeak);
	obs_source_t *pvw = pvwN.isEmpty() ? nullptr : obs_get_source_by_name(pvwN.toUtf8().constData());

	struct vec4 green, red, gray, black;
	vec4_set(&green, 0.16f, 0.70f, 0.34f, 1.0f);
	vec4_set(&red, 0.75f, 0.22f, 0.17f, 1.0f);
	vec4_set(&gray, 0.28f, 0.28f, 0.30f, 1.0f);
	vec4_set(&black, 0.04f, 0.04f, 0.04f, 1.0f);

	const int b = 2;
	Layout L = computeLayout((int)cx, (int)cy, (int)items.size());

	auto pane = [&](const QRect &r, obs_source_t *src, const struct vec4 &border, const QString &label) {
		mv_draw_box(r.x(), r.y(), r.width(), r.height(), border);
		mv_draw_box(r.x() + b, r.y() + b, r.width() - 2 * b, r.height() - 2 * b, black);
		mv_render_source(src, r.x() + b, r.y() + b, r.width() - 2 * b, r.height() - 2 * b);
		gs_texture_t *tex = labelTexture(label);
		if (tex)
			mv_draw_texture(tex, r.x() + b + 2,
					r.y() + r.height() - (int)gs_texture_get_height(tex) - b - 2);
	};

	pane(L.pvw, pvw, green, QStringLiteral("PREVIEW"));
	pane(L.pgm, pgm, red, QStringLiteral("PROGRAM"));

	for (int i = 0; i < L.thumbs.size() && i < (int)items.size(); i++) {
		const struct vec4 &bc = (items[i].name == pgmN) ? red : (items[i].name == pvwN) ? green : gray;
		pane(L.thumbs[i], items[i].src, bc, items[i].name);
	}

	for (Item &it : items)
		obs_source_release(it.src);
	obs_source_release(pgm);
	obs_source_release(pvw);
}

int MEMultiview::thumbAt(const QPoint &pos, QString &nameOut)
{
	std::vector<QString> names;
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		for (SceneRef &s : m_scenes)
			names.push_back(s.name);
	}
	Layout L = computeLayout(width(), height(), (int)names.size());
	for (int i = 0; i < L.thumbs.size() && i < (int)names.size(); i++) {
		if (L.thumbs[i].contains(pos)) {
			nameOut = names[i];
			return i;
		}
	}
	return -1;
}

void MEMultiview::mousePressEvent(QMouseEvent *e)
{
	QString name;
	if (thumbAt(e->pos(), name) >= 0)
		mv_set_preview(m_uuid, name);
}

void MEMultiview::mouseDoubleClickEvent(QMouseEvent *e)
{
	QString name;
	if (thumbAt(e->pos(), name) >= 0) {
		mv_set_preview(m_uuid, name);
		mv_auto(m_uuid);
	}
}

/* ---- Fenster-Verwaltung + Tools-Menü ------------------------------------ */

static QList<QPointer<MEMultiview>> g_windows;

static void open_multiview(const QString &uuid, const QString &name, int monitor)
{
	auto *mv = new MEMultiview(uuid, name);
	g_windows.append(mv);
	if (monitor >= 0) {
		QList<QScreen *> screens = QGuiApplication::screens();
		if (monitor < screens.size()) {
			mv->setGeometry(screens[monitor]->geometry());
			mv->showFullScreen();
			return;
		}
	}
	mv->resize(960, 540);
	mv->show();
}

static void rebuild_menu(QMenu *menu)
{
	menu->clear();
	QList<BankInfo> banks = mv_enum_banks();
	if (banks.isEmpty()) {
		QAction *a = menu->addAction(QStringLiteral("(keine Multi-M/E-Quelle)"));
		a->setEnabled(false);
		return;
	}
	const QList<QScreen *> screens = QGuiApplication::screens();
	for (const BankInfo &b : banks) {
		QMenu *sub = menu->addMenu(b.name.isEmpty() ? QStringLiteral("(unbenannt)") : b.name);
		const QString uuid = b.uuid, name = b.name;
		QObject::connect(sub->addAction(QStringLiteral("Fenster")), &QAction::triggered,
				 [uuid, name]() { open_multiview(uuid, name, -1); });
		for (int i = 0; i < screens.size(); i++) {
			QObject::connect(sub->addAction(QStringLiteral("Vollbild — Monitor %1").arg(i + 1)),
					 &QAction::triggered, [uuid, name, i]() { open_multiview(uuid, name, i); });
		}
	}
}

static void mv_on_frontend_event(enum obs_frontend_event event, void *data)
{
	UNUSED_PARAMETER(data);
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		for (QPointer<MEMultiview> &w : g_windows)
			if (w)
				w->close();
		g_windows.clear();
		obs_frontend_remove_event_callback(mv_on_frontend_event, NULL);
	}
}

void me_multiview_register(void)
{
	QAction *act = static_cast<QAction *>(obs_frontend_add_tools_menu_qaction("Multi-M/E Multiview"));
	if (act) {
		QMenu *menu = new QMenu();
		act->setMenu(menu);
		QObject::connect(menu, &QMenu::aboutToShow, [menu]() { rebuild_menu(menu); });
	}
	obs_frontend_add_event_callback(mv_on_frontend_event, nullptr);
	obs_log(LOG_INFO, "Multi-M/E multiview menu registered");
}
