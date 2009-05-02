/**
 * $Id$
 * Copyright (C) 2008 - 2009 Nils Asmussen
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <esc/common.h>
#include <esc/io.h>
#include <esc/proc.h>
#include <esc/gui/graphics.h>
#include <string.h>

namespace esc {
	namespace gui {
		void Graphics::drawLine(tCoord x0,tCoord y0,tCoord xn,tCoord yn) {
			s32	dx,	dy,	d;
			s32 incrE, incrNE;	/*Increments for move to E	& NE*/
			tCoord x,y;			/*Start & current pixel*/
			s32 incx, incy;
			tCoord *px, *py;

			x0 %= _width;
			xn %= _width;
			y0 %= _height;
			yn %= _height;

			updateMinMax(x0,y0);
			updateMinMax(xn,yn);

			// default settings
			x = x0;
			y = y0;
			px = &x;
			py = &y;
			dx = xn - x0;
			dy = yn - y0;
			incx = incy = 1;

			// mirror by x-axis ?
			if(dx < 0) {
				dx = -dx;
				incx = -1;
			}
			// mirror by y-axis ?
			if(dy < 0) {
				dy = -dy;
				incy = -1;
			}
			// handle 45° < x < 90°
			if(dx < dy) {
				swap<tCoord>(&x,&y);
				swap<tCoord*>(&px,&py);
				swap<s32>(&dx,&dy);
				swap<tCoord>(&x0,&y0);
				swap<tCoord>(&xn,&yn);
				swap<s32>(&incx,&incy);
			}

			d = 2 * dy - dx;
			/*Increments E & NE*/
			incrE = 2 * dy;
			incrNE = 2 * (dy - dx);

			for(x = x0; x != xn; x += incx) {
				doSetPixel(*px,*py);
				if(d < 0) {
					d += incrE;
				}
				else {
					d += incrNE;
					y += incy;
				}
			}
		}

		void Graphics::drawRect(tCoord x,tCoord y,tSize width,tSize height) {
			// top
			drawLine(x,y,x + width - 1,y);
			// right
			drawLine(x + width - 1,y,x + width - 1,y + height - 1);
			// bottom
			drawLine(x + width - 1,y + height - 1,x,y + height - 1);
			// left
			drawLine(x,y + height - 1,x,y);
		}

		void Graphics::fillRect(tCoord x,tCoord y,tSize width,tSize height) {
			validateParams(x,y,width,height);
			tCoord yend = y + height;
			updateMinMax(x,y);
			updateMinMax(x + width - 1,yend - 1);
			if(_col == 0) {
				u8 *mem = _pixels + y * _width + x;
				u32 psize = _pixel->getPixelSize();
				u32 inc = _width * psize;
				u32 count = width * psize;
				for(; y < yend; y++) {
					memset(mem,_col,count);
					mem += inc;
				}
			}
			else {
				tCoord xcur;
				tCoord xend = x + width;
				for(; y < yend; y++) {
					for(xcur = x; xcur < xend; xcur++)
						doSetPixel(xcur,y);
				}
			}
		}

		void Graphics::update() {
			update(_minx,_miny,_maxx - _minx + 1,_maxy - _miny + 1);

			// reset region
			_minx = _width - 1;
			_maxx = 0;
			_miny = _height - 1;
			_maxy = 0;
		}

		void Graphics::update(tCoord x,tCoord y,tSize width,tSize height) {
			validateParams(x,y,width,height);
			// is there anything to update?
			if(width > 0 || height > 0) {
				width = MIN(_width - x,width);
				height = MIN(_height - y,height);
				void *vesaMem = Application::getInstance()->getVesaMem();
				u8 *src,*dst;
				tCoord endy = y + height;
				u32 psize = _pixel->getPixelSize();
				u32 count = width * psize;
				src = _pixels + (y * _width + x) * psize;
				dst = (u8*)vesaMem + ((_y + y) * RESOLUTION_X + (_x + x)) * psize;
				while(y < endy) {
					memcpy(dst,src,count);
					src += _width * psize;
					dst += RESOLUTION_X * psize;
					y++;
				}

				notifyVesa(_x + x,_y + endy - height,width,height);
				yield();
			}
		}

		void Graphics::clear() {
			u16 psize = _pixel->getPixelSize();
			tCoord y = _y;
			tCoord maxy = y + _height;
			tCoord count = (1 + _width) * psize;
			u8 *vesaMem = (u8*)Application::getInstance()->getVesaMem();
			vesaMem += (y * RESOLUTION_X + _x) * psize;
			while(y <= maxy) {
				memset(vesaMem,0,count);
				vesaMem += RESOLUTION_X * psize;
				y++;
			}
			notifyVesa(_x,_y,_width,_height);
		}

		void Graphics::notifyVesa(tCoord x,tCoord y,tSize width,tSize height) {
			tFD vesaFd = Application::getInstance()->getVesaFd();
			_vesaMsg.data.x = x;
			_vesaMsg.data.y = y;
			_vesaMsg.data.width = width;
			_vesaMsg.data.height = height;
			write(vesaFd,&_vesaMsg,sizeof(sMsgVesaUpdate));
		}

		void Graphics::move(tCoord x,tCoord y) {
			_x = MIN(x,RESOLUTION_X - _width - 1);
			_y = MIN(y,RESOLUTION_Y - _height - 1);
		}

		void Graphics::debug() const {
			for(tCoord y = 0; y < _height; y++) {
				for(tCoord x = 0; x < _width; x++)
					out << (getPixel(x,y) == 0 ? ' ' : 'x');
				out << endl;
			}
		}

		void Graphics::validateParams(tCoord &x,tCoord &y,tSize &width,tSize &height) {
			x %= _width;
			y %= _height;
			if(x + width >= _width)
				width = _width - x;
			if(y + height >= _height)
				height = _height - y;
		}
	}
}
