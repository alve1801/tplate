/* TPlate rasterizer
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

int abs(int x){return(x>0?x:-x);}

// which continent is at the current pixel. WAY easier than raycasting. also,
//  has an offset of 1, so we can use 0 to denote blanks.
int zbuf[height*width];

void clear(){for(int i=0;i<height*width;i++) zbuf[i]=0;}

void put(Framework*w,_color col,int ind,int x,int y){
	if(x<0 || x>=width || y<0 || y>=height) return; // XXX optimize below!
	if(ind || !zbuf[y*width+x]) {w->p(col,x,y); zbuf[y*width+x]=ind;}
}

#define getx(i) verts[mod(i,vnum)*2]
#define gety(i) verts[mod(i,vnum)*2+1]

void tess(Framework*w,int ind,_color col,int*verts,int vnum){
	struct lseg{int from,to, x,s, dx,dy,err; lseg*next;
		lseg(int a,int b,int*v,lseg*n=0) : from(a),to(b),err(0),next(n){
			x=v[a*2];
			dx=abs(v[a*2]-v[b*2]);
			dy=abs(v[a*2+1]-v[b*2+1]);
			s=(v[a*2]<v[b*2])?1:-1; // irrelevant if dy==0
		}~lseg() {if(next) delete next;}
	}*line=0;

	if(vnum<3) return; // size culling

	// figure out bounding box
	int bbox[4]={10000,10000,0,0}; // minx,miny,maxx,maxy
	for(int i=0;i<vnum;i++){
		if(getx(i)<bbox[0]) bbox[0]=getx(i);
		if(getx(i)>bbox[2]) bbox[2]=getx(i);
		if(gety(i)<bbox[1]) bbox[1]=gety(i);
		if(gety(i)>bbox[3]) bbox[3]=gety(i);
	}

	// view culling
	if(bbox[0]>width || bbox[1]>height || bbox[2]<0 || bbox[3]<0) return;

	// adjust color for backface
	if(!ind) col=(col>>1)&0x777;

	// foreach line in bbox
	for(int y=bbox[1];y<bbox[3];y++){
		// figure out if it crosses any verts
		for(int i=0;i<vnum;i++) if(gety(i)==y){
			// if we dont have any segments, instantiate w/out all the testing
			if(!line){
				if(gety(i)!=gety(i-1))
					line=new lseg(i,mod(i-1,vnum),verts);
				if(gety(i)!=gety(i+1)){
					if(!line) line=new lseg(i,mod(i+1,vnum),verts);
					else if(getx(i-1)<getx(i+1))
						line->next=new lseg(i,mod(i+1,vnum),verts);
					else line=new lseg(i,mod(i+1,vnum),verts,line);
				}continue; // skip below shenanigans
			}int lx,ly;

			// is previous edge above or below current scanline?
			ly=gety(i-1);
			if(ly>y){ // below scanline, add new edge
				lx=getx(i); // vert horizontal pos
				if(line->x>lx) line=new lseg(i,mod(i-1,vnum),verts,line);
				else for(lseg*t=line;t;t=t->next)
				if(t->x<=lx && (!t->next || t->next->x>=lx)){
					t->next=new lseg(i,mod(i-1,vnum),verts,t->next);
					break;
			}}
			if(ly<y){ // above scanline, find matching edge(s) and delete
				lseg*t=line, *prev=0, *tmp;
				while(t) if(t->to!=i) prev=t, t=t->next; else{
					tmp=t; t=t->next;
					if(prev) prev->next=t; else line=t;
					tmp->next=0; delete tmp;
				}
			}

			// likewise for following edge
			ly=gety(i+1);
			if(ly>y){ // below scanline, add new edge
				lx=getx(i); // vert horizontal pos
				if(line->x>lx) line=new lseg(i,mod(i+1,vnum),verts,line);
				else for(lseg*t=line;t;t=t->next)
				if(t->x<=lx && (!t->next || t->next->x>=lx)){
					t->next=new lseg(i,mod(i+1,vnum),verts,t->next);
					break;
			}}
			if(ly<y){ // above scanline, find matching edge(s) and delete
				lseg*t=line, *prev=0, *tmp;
				while(t) if(t->to!=i) prev=t, t=t->next; else{
					tmp=t; t=t->next;
					if(prev) prev->next=t; else line=t;
					tmp->next=0; delete tmp;
				}
			}

			/*printf("edges are:\n");
			for(lseg*t=line;t;t=t->next)
				printf(" %i %i->%i (%i/%i,%i %c)\n",
					t->x,t->from,t->to,t->dx,t->dy,t->err,t->s==-1?'-':'+');*/

		}

		// rasterize line
		for(lseg*t=line;t;t=t->next){
			int x=t->x; t=t->next;
			if(!t) {printf("ERR! - rast\n"); break;}
			//while(x<t->x) w->p(col,x++,y);
			while(x<t->x) put(w,col,ind,x++,y);
		}

		// step bresenham
		for(lseg*t=line;t;t=t->next) if(t->dx){
			if(!t->dy) {printf("ERR! - bres\n"); break;}
			t->err+=t->dx;
			while(t->err>t->dy) t->err-=t->dy, t->x+=t->s;
		}

		// hacky gnomesort to resolve any nonvertex line crossings
		{lseg*tsort[100]; int at=1,max=0;
		for(lseg*t=line;t;t=t->next) tsort[max++]=t;
		tsort[max]=0; while(at<max){
			if(at==0 || tsort[at]->x>=tsort[at-1]->x) at++;
			else {lseg*t=tsort[at]; tsort[at]=tsort[at-1]; tsort[at-1]=t; at--;}
		}line=tsort[0]; for(at=0;tsort[at];at++) tsort[at]->next=tsort[at+1];}
	}

	// clean up
	if(line) delete line;
}

#undef getx
#undef gety
