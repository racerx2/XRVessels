/**
  XR Vessel add-ons for OpenOrbiter Space Flight Simulator - Linux port
  Copyright (C) 2026 racerx2
  Based on the XR vessels, Copyright (C) 2006-2025 Douglas Beachy

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
**/

// not upstream: GDI counterparts on QPainter for the XR panel code (Linux port)

#include "XRGdi.h"
#include <QFontMetrics>
#include <QPolygon>
#include <unordered_map>
#include <cstdlib>
#include <cmath>
#include <algorithm>

namespace {

QColor ToQColor (COLORREF c)
{
	return QColor (GetRValue (c), GetGValue (c), GetBValue (c));
}

// GDI DC defaults: system font, black 1-pixel pen, white brush, black text on white, OPAQUE, TA_LEFT|TA_TOP, (0,0)
struct GdiDC {
	QFont *font = nullptr;
	QPen *pen = nullptr;
	QBrush *brush = nullptr;
	COLORREF textColor = RGB(0,0,0);
	COLORREF bkColor = RGB(255,255,255);
	int bkMode = OPAQUE;
	UINT textAlign = TA_LEFT | TA_TOP;
	QPoint pos;
};

std::unordered_map<QPainter*, GdiDC> g_dc; // one thread: Orbiter's main loop

QPen g_blackPen (QColor (0,0,0), 1), g_whitePen (QColor (255,255,255), 1), g_nullPen (Qt::NoPen);
QBrush g_whiteBrush (QColor (255,255,255)), g_ltgrayBrush (QColor (192,192,192)), g_grayBrush (QColor (128,128,128)),
	g_dkgrayBrush (QColor (64,64,64)), g_blackBrush (QColor (0,0,0)), g_nullBrush (Qt::NoBrush);

GdiDC &State (QPainter *hDC)
{
	auto it = g_dc.find (hDC);
	if (it == g_dc.end ()) {
		it = g_dc.emplace (hDC, GdiDC ()).first;
		it->second.pen = &g_blackPen;
		it->second.brush = &g_whiteBrush;
	}
	return it->second;
}

bool IsStock (const void *p)
{
	return p == &g_blackPen || p == &g_whitePen || p == &g_nullPen || p == &g_whiteBrush || p == &g_ltgrayBrush ||
		p == &g_grayBrush || p == &g_dkgrayBrush || p == &g_blackBrush || p == &g_nullBrush;
}

XRGdiObj Obj (QFont *f) { XRGdiObj o (f); return o; }
XRGdiObj Obj (QPen *p) { XRGdiObj o (p); o.stock = IsStock (p); return o; }
XRGdiObj Obj (QBrush *b) { XRGdiObj o (b); o.stock = IsStock (b); return o; }

} // namespace

QFont *CreateFont (int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strikeout,
	DWORD charset, DWORD outprec, DWORD clipprec, DWORD quality, DWORD pitchfamily, const char *face)
{
	QString family = QString::fromLatin1 (face ? face : "");
	if (family.compare ("Microsoft Sans Serif", Qt::CaseInsensitive) == 0)
		family = "Liberation Sans"; // Arial metrics as MS Sans Serif; fontconfig would pick a wider face
	QFont *font = new QFont (family);
	if ((pitchfamily & 0x3) == FIXED_PITCH) font->setFixedPitch (true);
	if (weight > 0) font->setWeight (QFont::Weight (std::clamp (weight, 100, 900)));
	font->setItalic (italic != 0);
	font->setUnderline (underline != 0);
	font->setStrikeOut (strikeout != 0);
	if (quality == NONANTIALIASED_QUALITY) font->setStyleStrategy (QFont::NoAntialias);
	if (h < 0) font->setPixelSize (-h);
	else if (h > 0) { // cell height: scale the em so ascent + descent come to h
		font->setPixelSize (h);
		int cell = QFontMetrics (*font).height ();
		if (cell > 0 && cell != h) font->setPixelSize (std::max (1, (int)std::lround ((double)h * h / cell)));
	}
	return font;
}

QPen *CreatePen (int style, int width, COLORREF col)
{
	if (style == PS_NULL) return new QPen (Qt::NoPen);
	QPen *pen = new QPen (ToQColor (col), std::max (width, 1));
	pen->setStyle (style == PS_DASH ? Qt::DashLine : style == PS_DOT ? Qt::DotLine : style == PS_DASHDOT ? Qt::DashDotLine :
		style == PS_DASHDOTDOT ? Qt::DashDotDotLine : Qt::SolidLine);
	pen->setCapStyle (Qt::RoundCap); // GDI pens: round ends and joins
	pen->setJoinStyle (Qt::RoundJoin);
	return pen;
}

QBrush *CreateSolidBrush (COLORREF col)
{
	return new QBrush (ToQColor (col));
}

XRGdiObj GetStockObject (int id)
{
	switch (id) {
	case WHITE_BRUSH:  return Obj (&g_whiteBrush);
	case LTGRAY_BRUSH: return Obj (&g_ltgrayBrush);
	case GRAY_BRUSH:   return Obj (&g_grayBrush);
	case DKGRAY_BRUSH: return Obj (&g_dkgrayBrush);
	case BLACK_BRUSH:  return Obj (&g_blackBrush);
	case NULL_BRUSH:   return Obj (&g_nullBrush);
	case WHITE_PEN:    return Obj (&g_whitePen);
	case BLACK_PEN:    return Obj (&g_blackPen);
	case NULL_PEN:     return Obj (&g_nullPen);
	}
	return XRGdiObj ();
}

XRGdiObj SelectObject (QPainter *hDC, QFont *font)
{
	GdiDC &dc = State (hDC);
	XRGdiObj prev = Obj (dc.font);
	dc.font = font;
	if (font) hDC->setFont (*font);
	return prev;
}

XRGdiObj SelectObject (QPainter *hDC, QPen *pen)
{
	GdiDC &dc = State (hDC);
	XRGdiObj prev = Obj (dc.pen);
	if (pen) dc.pen = pen;
	return prev;
}

XRGdiObj SelectObject (QPainter *hDC, QBrush *brush)
{
	GdiDC &dc = State (hDC);
	XRGdiObj prev = Obj (dc.brush);
	if (brush) dc.brush = brush;
	return prev;
}

XRGdiObj SelectObject (QPainter *hDC, XRGdiObj obj)
{
	switch (obj.kind) {
	case XRGdiObj::FONT:  return SelectObject (hDC, static_cast<QFont*>(obj));
	case XRGdiObj::PEN:   return SelectObject (hDC, static_cast<QPen*>(obj));
	case XRGdiObj::BRUSH: return SelectObject (hDC, static_cast<QBrush*>(obj));
	default: return XRGdiObj ();
	}
}

// a deleted object still selected in a DC falls back to the DC default there (GDI defers the delete)
BOOL DeleteObject (QFont *font)
{
	for (auto &e : g_dc) if (e.second.font == font) e.second.font = nullptr;
	delete font;
	return TRUE;
}

BOOL DeleteObject (QPen *pen)
{
	if (IsStock (pen)) return TRUE;
	for (auto &e : g_dc) if (e.second.pen == pen) e.second.pen = &g_blackPen;
	delete pen;
	return TRUE;
}

BOOL DeleteObject (QBrush *brush)
{
	if (IsStock (brush)) return TRUE;
	for (auto &e : g_dc) if (e.second.brush == brush) e.second.brush = &g_whiteBrush;
	delete brush;
	return TRUE;
}

BOOL DeleteObject (XRGdiObj obj)
{
	if (obj.stock) return TRUE;
	switch (obj.kind) {
	case XRGdiObj::FONT:  return DeleteObject (static_cast<QFont*>(obj));
	case XRGdiObj::PEN:   return DeleteObject (static_cast<QPen*>(obj));
	case XRGdiObj::BRUSH: return DeleteObject (static_cast<QBrush*>(obj));
	default: return FALSE;
	}
}

COLORREF SetTextColor (QPainter *hDC, COLORREF col)
{
	GdiDC &dc = State (hDC);
	COLORREF prev = dc.textColor;
	dc.textColor = col;
	return prev;
}

COLORREF SetBkColor (QPainter *hDC, COLORREF col)
{
	GdiDC &dc = State (hDC);
	COLORREF prev = dc.bkColor;
	dc.bkColor = col;
	return prev;
}

int SetBkMode (QPainter *hDC, int mode)
{
	GdiDC &dc = State (hDC);
	int prev = dc.bkMode;
	dc.bkMode = mode;
	return prev;
}

UINT SetTextAlign (QPainter *hDC, UINT align)
{
	GdiDC &dc = State (hDC);
	UINT prev = dc.textAlign;
	dc.textAlign = align;
	return prev;
}

BOOL TextOut (QPainter *hDC, int x, int y, const char *str, int len)
{
	GdiDC &dc = State (hDC);
	if (dc.textAlign & TA_UPDATECP) { x = dc.pos.x (); y = dc.pos.y (); } // GDI draws at the current position
	if (dc.font) hDC->setFont (*dc.font);
	QString s = QString::fromLatin1 (str, len);
	QFontMetrics fm = hDC->fontMetrics ();
	int w = fm.horizontalAdvance (s);
	int x0 = ((dc.textAlign & TA_CENTER) == TA_CENTER ? x - w/2 : (dc.textAlign & TA_RIGHT) ? x - w : x);
	int ytop = ((dc.textAlign & TA_BASELINE) == TA_BASELINE ? y - fm.ascent () : (dc.textAlign & TA_BOTTOM) ? y - fm.height () : y);
	if (dc.bkMode == OPAQUE)
		hDC->fillRect (x0, ytop, w, fm.height (), ToQColor (dc.bkColor));
	hDC->setPen (ToQColor (dc.textColor)); // GDI text colour is apart from the pen
	hDC->drawText (QPoint (x0, ytop + fm.ascent ()), s);
	if (dc.textAlign & TA_UPDATECP) dc.pos.setX (x0 + w);
	return TRUE;
}

BOOL MoveToEx (QPainter *hDC, int x, int y, POINT *prev)
{
	GdiDC &dc = State (hDC);
	if (prev) { prev->x = dc.pos.x (); prev->y = dc.pos.y (); }
	dc.pos = QPoint (x, y);
	return TRUE;
}

BOOL LineTo (QPainter *hDC, int x, int y)
{
	GdiDC &dc = State (hDC);
	QPoint p0 = dc.pos, p1 (x, y);
	dc.pos = p1;
	if (!dc.pen || dc.pen->style () == Qt::NoPen || p0 == p1) return TRUE;
	hDC->setRenderHint (QPainter::Antialiasing, false);
	if (dc.pen->width () <= 1 && dc.pen->style () == Qt::SolidLine) {
		// GDI leaves out the last pixel: Bresenham from p0 up to but not including p1
		hDC->setPen (QPen (dc.pen->color (), 1));
		int dx = std::abs (p1.x () - p0.x ()), sx = p0.x () < p1.x () ? 1 : -1;
		int dy = -std::abs (p1.y () - p0.y ()), sy = p0.y () < p1.y () ? 1 : -1;
		int err = dx + dy, cx = p0.x (), cy = p0.y ();
		while (cx != p1.x () || cy != p1.y ()) {
			hDC->drawPoint (cx, cy);
			int e2 = 2 * err;
			if (e2 >= dy) { err += dy; cx += sx; }
			if (e2 <= dx) { err += dx; cy += sy; }
		}
	} else {
		hDC->setPen (*dc.pen);
		hDC->drawLine (p0, p1);
	}
	return TRUE;
}

BOOL Rectangle (QPainter *hDC, int left, int top, int right, int bottom)
{
	GdiDC &dc = State (hDC);
	if (right <= left || bottom <= top) return TRUE;
	hDC->setRenderHint (QPainter::Antialiasing, false);
	if (dc.brush && dc.brush->style () != Qt::NoBrush)
		hDC->fillRect (QRect (left, top, right - left, bottom - top), *dc.brush);
	if (dc.pen && dc.pen->style () != Qt::NoPen) { // outline inside [left,right) x [top,bottom)
		hDC->setPen (*dc.pen);
		hDC->setBrush (Qt::NoBrush);
		hDC->drawRect (QRect (left, top, right - left - 1, bottom - top - 1));
	}
	return TRUE;
}

int FillRect (QPainter *hDC, const RECT *rc, QBrush *brush)
{
	if (!rc || !brush) return 0;
	hDC->fillRect (QRect (rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top), *brush);
	return 1;
}

BOOL Polygon (QPainter *hDC, const POINT *pt, int n)
{
	GdiDC &dc = State (hDC);
	QPolygon poly;
	for (int i = 0; i < n; i++) poly << QPoint (pt[i].x, pt[i].y);
	hDC->setRenderHint (QPainter::Antialiasing, false);
	hDC->setPen (dc.pen ? *dc.pen : QPen (Qt::NoPen));
	hDC->setBrush (dc.brush ? *dc.brush : QBrush (Qt::NoBrush));
	hDC->drawPolygon (poly, Qt::OddEvenFill); // GDI ALTERNATE fill mode
	return TRUE;
}

void GdiResetDC (QPainter *hDC)
{
	g_dc.erase (hDC);
}

void GdiReleaseDC (QPainter *hDC)
{
	g_dc.erase (hDC);
}
