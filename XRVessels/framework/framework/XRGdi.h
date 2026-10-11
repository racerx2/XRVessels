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
// QPainter lacks the GDI DC state (text colour apart from the pen, text alignment, current position, background
// mode and colour), so it is kept per painter; Area::GetDC resets it and Area::ReleaseDC drops it.

#pragma once

#include "OrbiterAPI.h"
#include <QPainter>
#include <QFont>
#include <QPen>
#include <QBrush>
#include <QImage>

// GDI constants, Win32 values
#define TRANSPARENT 1
#define OPAQUE      2

#define TA_NOUPDATECP 0
#define TA_UPDATECP   1
#define TA_LEFT       0
#define TA_RIGHT      2
#define TA_CENTER     6
#define TA_TOP        0
#define TA_BOTTOM     8
#define TA_BASELINE   24

#define PS_SOLID      0
#define PS_DASH       1
#define PS_DOT        2
#define PS_DASHDOT    3
#define PS_DASHDOTDOT 4
#define PS_NULL       5

#define FW_DONTCARE   0
#define FW_THIN       100
#define FW_LIGHT      300
#define FW_NORMAL     400
#define FW_MEDIUM     500
#define FW_SEMIBOLD   600
#define FW_BOLD       700
#define FW_HEAVY      900

#define ANSI_CHARSET    0
#define DEFAULT_CHARSET 1
#define OUT_DEFAULT_PRECIS  0
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY        0
#define DRAFT_QUALITY          1
#define PROOF_QUALITY          2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY    4
#define DEFAULT_PITCH  0
#define FIXED_PITCH    1
#define VARIABLE_PITCH 2
#define FF_DONTCARE    (0<<4)
#define FF_ROMAN       (1<<4)
#define FF_SWISS       (2<<4)
#define FF_MODERN      (3<<4)

#define WHITE_BRUSH  0
#define LTGRAY_BRUSH 1
#define GRAY_BRUSH   2
#define DKGRAY_BRUSH 3
#define BLACK_BRUSH  4
#define NULL_BRUSH   5
#define HOLLOW_BRUSH NULL_BRUSH
#define WHITE_PEN    6
#define BLACK_PEN    7
#define NULL_PEN     8

// HGDIOBJ counterpart: a selected or stock font, pen or brush
struct XRGdiObj {
	enum Kind { NONE, FONT, PEN, BRUSH } kind = NONE;
	void *obj = nullptr;
	bool stock = false;
	XRGdiObj () {}
	XRGdiObj (QFont *f): kind(f ? FONT : NONE), obj(f) {}
	XRGdiObj (QPen *p): kind(p ? PEN : NONE), obj(p) {}
	XRGdiObj (QBrush *b): kind(b ? BRUSH : NONE), obj(b) {}
	operator QFont * () const { return kind == FONT ? static_cast<QFont*>(obj) : nullptr; }
	operator QPen * () const { return kind == PEN ? static_cast<QPen*>(obj) : nullptr; }
	operator QBrush * () const { return kind == BRUSH ? static_cast<QBrush*>(obj) : nullptr; }
	explicit operator bool () const { return obj != nullptr; }
};

// CreateFont: h < 0 character (em) height, h > 0 cell height, as GDI
QFont *CreateFont (int h, int w, int esc, int orient, int weight, DWORD italic, DWORD underline, DWORD strikeout,
	DWORD charset, DWORD outprec, DWORD clipprec, DWORD quality, DWORD pitchfamily, const char *face);
QPen *CreatePen (int style, int width, COLORREF col);
QBrush *CreateSolidBrush (COLORREF col);
XRGdiObj GetStockObject (int id);

// SelectObject returns the object of the same kind selected before
XRGdiObj SelectObject (QPainter *hDC, QFont *font);
XRGdiObj SelectObject (QPainter *hDC, QPen *pen);
XRGdiObj SelectObject (QPainter *hDC, QBrush *brush);
XRGdiObj SelectObject (QPainter *hDC, XRGdiObj obj);

BOOL DeleteObject (QFont *font);
BOOL DeleteObject (QPen *pen);
BOOL DeleteObject (QBrush *brush);
BOOL DeleteObject (XRGdiObj obj);

COLORREF SetTextColor (QPainter *hDC, COLORREF col);
COLORREF SetBkColor (QPainter *hDC, COLORREF col);
int SetBkMode (QPainter *hDC, int mode);
UINT SetTextAlign (QPainter *hDC, UINT align);

BOOL TextOut (QPainter *hDC, int x, int y, const char *str, int len);
BOOL MoveToEx (QPainter *hDC, int x, int y, POINT *prev);
BOOL LineTo (QPainter *hDC, int x, int y);
BOOL Rectangle (QPainter *hDC, int left, int top, int right, int bottom);
int FillRect (QPainter *hDC, const RECT *rc, QBrush *brush);
BOOL Polygon (QPainter *hDC, const POINT *pt, int n);

// DC state lifetime: reset when a DC is obtained, dropped before it is released
void GdiResetDC (QPainter *hDC);
void GdiReleaseDC (QPainter *hDC);
