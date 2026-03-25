/* mapmaker drawing program
 Copyright (C) 2026 Ave Tealeaf

This file is part of TPlate.

TPlate is free software: you can redistribute and/or modify it under the terms
 of the GNU General Public License, version 3 or later, as published by the
 Free Software Foundation.

This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more
 details.

You should have received a copy of the GNU General Public License along with
 this program. If not, see <https://www.gnu.org/licenses/>. */

#include<stdio.h>
#include<math.h> // boy are we gonna need this one
#include"libv3.h"

// map stuff
struct v2{float a,b;v2(float a=0,float b=0):a(a),b(b){}};
img map,bkg;
float lon,lat,dst=0;bool draw; // extra return value

v2 to(int x,int y){ // from x/y to lon/lat
	float a=x/200.0-1,b=y/200.0-1,p=sqrtf(a*a+b*b),c,a1,b1;
	if(p<1){draw=1;c=asinf(p)/dst;
		a1=lon+atan2f(p*cosf(c)*cosf(lat)-b*sinf(c)*sinf(lat),a*sinf(c));
		b1=asinf(cosf(c)*sinf(lat)+b*sinf(c)*cosf(lat)/p);
		return v2(fmodf(a1*M_1_PI*-.5+1,1),fmodf(b1*M_1_PI+1.5,1));
	}draw=0;return v2();
}



// ui stuff
short color;int bsize=0;
const short palette[7]={0,0x888,0x04f,0x0a2,0xfcb,0xc02,0};
struct Widget{
	int x,y,sx,sy,t; // colorpicks layers latlong saveimg brush
	Widget(int x,int y,int t):x(x),y(y),t(t){
		switch(t){
		case 0:sx=64,sy=128;break;
		case 2:sx=sy=32*8;break;
		case 3:sx=12*8,sy=8;break;
		case 4:break;}}

	void draw(Framework*w){
		switch(t){
		case 0:{ // colorpicks
			// draw color picks
			for(int y=0;y<128;y++){
				unsigned char ty=y>>3;int x;
				//for(x= 0;x<16;x++)w->p(color,x,y);
				for(x=20;x<28;x++)w->p(ty<<8 | color&0x0ff,x,y);
				for(x=36;x<44;x++)w->p(ty<<4 | color&0xf0f,x,y);
				for(x=52;x<60;x++)w->p(ty    | color&0xff0,x,y);
			}

			for(int cx=0;cx<16;cx++)for(int cy=0;cy<16;cy++){
				w->p(color,x+cx,y+cy);
				for(int col=0;col<7;col++)
				w->p(palette[col],x+cx,y+cy+16+col*16);
			}

			const char*colorsel="[]";
			w->ps(colorsel,2,(color&0xf00)>>8,0xfff,0xf0f);
			w->ps(colorsel,4,(color&0x0f0)>>4,0xfff,0xf0f);
			w->ps(colorsel,6,(color&0x00f)>>0,0xfff,0xf0f);

		}break;case 2:{ // latlong
			const int res=32; // resolution
			for(int i=0;i<res;i++)
				w->pc(i%4?'|':'+',x>>3,(y>>3)+i),
				w->pc(i%4?'-':'+',(x>>3)+i+1,(y>>3)+res);

			int px=(lat*M_1_PI+.5)*res,py=res-(lon*M_1_PI*.5)*res;
			py-=res>>2;if(py<0)py+=res;
			w->pc('<',(x>>3)+1,(y>>3)+px);
			w->pc('v',(x>>3)+py+1,(y>>3)+res-1);

			w->pi((int)(dst*100),(x>>3)+4,(y>>3)+res-3);
			w->ps("zoom w/ []",(x>>3)+8,(y>>3)+res-3);

		}break;case 3:{w->ps("[save image]",(x>>3),(y>>3));
		}break;case 4:{ // brush size
			w->ps("brush size:\nuse -+",x>>3,y>>3);
			w->pi(bsize+1,(x>>3)+12,y>>3);
		}break;}
	}

	bool tick(Framework*w){
		int mx=w->mouse.x,my=w->mouse.y;
		if(x<mx && mx<x+sx && y<my && my<y+sy){switch(t){
			case 0:{ // colorpicks
				int x=w->mouse.x,tilesize=16;
				char ty=w->mouse.y*2/tilesize;
				//   if(x<tilesize  ) color = 0xfff; // transparency
				     if(x<tilesize  ){if(ty>1)color = palette[(ty>>1)-1];}
				else if(x<tilesize*2){color = color&0x0ff | ty<<8;} // select red
				else if(x<tilesize*3){color = color&0xf0f | ty<<4;} // select green
				else if(x<tilesize*4){color = color&0xff0 | ty<<0;} // select blue
			}break;case 2:{ // coordinate selector
				const int res=32; // XXX ideally, combine both definitions?
				mx-=x,my-=y; // XXX also, dont like the return mess here
				if(0<mx && mx<16){
					lat=((float)(my>>3)/res-.5)*M_PI;
					return 1;
				}if(res*8-16<my && my<res*8){
					lon=((float)(mx>>3)/res*-2+1)*M_PI;
					return 1;
				}return 0;

			}break;case 3:{map.save("out.ppm");printf("image saved\n");}
		}return 1;}
		return 0;
	}
};
const int max_widget=10;
Widget*widgets[max_widget];



void ppi(int x,int y){int b=1<<bsize;
	for(int dx=1-b;dx<b;dx++)for(int dy=1-b;dy<b;dy++)
		map.at(x+dx,y+dy)=color;}

bool userfunc(Framework*w){
	if(w->keyp(SDL_SCANCODE_ESCAPE))return 0;

	// inputs
	bool widget=0;
	if(w->mouse.lp)for(int i=0;i<max_widget;i++)
		if(widgets[i] && widgets[i]->tick(w)){widget=1;break;}
	static int ox=0,oy=0;
	if(!widget && w->mouse.lh){ // draw
		v2 at=to(w->mouse.x-120,w->mouse.y-40);
		int ax=at.a*map.sx, ay=at.b*map.sy;
		if(draw){if(ox==0||oy==0)ppi(ax,ay);else{
			int dx=ax-ox, dy=ay-oy, dist=max(abs(dx),abs(dy));
			for(int i=0;i<dist;i++)ppi(ox+dx*i/dist, oy+dy*i/dist);
		}}ox=ax,oy=ay; // poor man's bresenham
	}else ox=oy=0;

	float inc=.01;
	if(w->keyh(SDL_SCANCODE_LSHIFT))inc=.1;
	// unfortunately, the devine trick doesnt work, so we have to do this
	if(w->keyh(SDL_SCANCODE_LCTRL)){
		if(w->keyp(SDL_SCANCODE_A))lon+=inc;
		if(w->keyp(SDL_SCANCODE_D))lon-=inc;
		if(w->keyp(SDL_SCANCODE_W) && lat > -M_PI_2+inc)lat-=inc;
		if(w->keyp(SDL_SCANCODE_S) && lat <  M_PI_2-inc)lat+=inc;
	}else{
		if(w->keyh(SDL_SCANCODE_A))lon+=inc;
		if(w->keyh(SDL_SCANCODE_D))lon-=inc;
		if(w->keyh(SDL_SCANCODE_W) && lat > -M_PI_2+inc)lat-=inc;
		if(w->keyh(SDL_SCANCODE_S) && lat <  M_PI_2-inc)lat+=inc;
	}lon=fmodf(lon,2*M_PI);if(lon<0)lon+=2*M_PI; // istg

	if(w->keyp(SDL_SCANCODE_MINUS) && bsize)bsize--;
	if(w->keyp(SDL_SCANCODE_EQUALS) && bsize<10)bsize++;
	if(w->keyh(SDL_SCANCODE_LEFTBRACKET))dst*=.95;
	if(w->keyh(SDL_SCANCODE_RIGHTBRACKET))dst/=.95;
	if(dst<1)dst=1;

	if(w->keyp(SDL_SCANCODE_1))color=palette[0];
	if(w->keyp(SDL_SCANCODE_2))color=palette[1];
	if(w->keyp(SDL_SCANCODE_3))color=palette[2];
	if(w->keyp(SDL_SCANCODE_4))color=palette[3];
	if(w->keyp(SDL_SCANCODE_5))color=palette[4];
	if(w->keyp(SDL_SCANCODE_6))color=palette[5];

	if(w->keyp(SDL_SCANCODE_L))printf("%.2f %.2f\n",lon,lat);
	if(w->keyp(SDL_SCANCODE_O)){map.save("out.ppm");printf("image saved\n");}

	// graphics
	for(int i=0;i<max_widget;i++)if(widgets[i])widgets[i]->draw(w);
	for(int x=0;x<400;x++)for(int y=0;y<400;y++){
		v2 at=to(x,y);
		if(draw){
			if(bkg.data && x&y&1){
				w->p(bkg[(int)(at.b*bkg.sy)*bkg.sx+at.a*bkg.sx],x+120,y+40);
			}else
				w->p(map[(int)(at.b*map.sy)*map.sx+at.a*map.sx],x+120,y+40);
	}}

	w->p(0xf00,320,240); // reticle
	w->p(0xf00,320,238);w->p(0xf00,320,242);
	w->p(0xf00,318,240);w->p(0xf00,322,240);

	if(w->keyp(SDL_SCANCODE_P))w->snap();return 1;
}

int main(int argc,char**argv){
	if(argc>1)map.load(argv[1]);else map.load("map.ppm");
	if(!map.data){printf("could not open file\n");return -1;}

	if(argc>2)bkg.load(argv[2]);

	lon=1.2,lat=-.7;
	color=0x27f;

	widgets[0] = new Widget(0,0,0);
	widgets[1] = new Widget(0,480-32*8-8,2);
	widgets[2] = new Widget(640-16*8,3*8,3);
	widgets[4] = new Widget(64,0,4);

	Framework w(640,480,2);w.transparent=0xf0f;
	w.run(userfunc);return 0;
}

/* yet another drawing program, but this time were drawing maps, which means
 we have to deal w/ projective distortion n shit

optimise math some

layers. layers would be grand.
even if i just specify two files on the cmdline and it does some dither shit
 to display one through the other, itd be grand for overlays n shit

can it work w/ a drawing tablet?
kinda. i should really make it interpolate mouse positions tho

would be neat if i could combine it w/ the height renderer, but thatd need
 heightmap trickery

*/
