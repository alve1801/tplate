/* TPlate tectonics tracker
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
#include<math.h> // not using this as much as i expected tbh
#include<vector>
using std::vector; // ugh
#include"libv3.h"

//const int width=640, height=480;
const int width=960, height=720;
//const int width=1280, height=960;
float scale=height*.55;

#include"parse.h" // from spc, for savefiles
#include"tess.h" // rasterizer
#include"q.h" // from spc, for trig stuff
#include"helptext.h" // long-ass string istg

vector<int>pts; // used for dynamic functions (pts in screen space)
struct tst{int at; tst*next; // gui indicator for keyframes (cached)
	tst(int a,tst*n):at(a),next(n){} ~tst(){ if(next) delete next; next=0;}
} tstmps(0,0);
int tseg; // indicate where on the coast to start rifting (cached for ui)

img underlay; // didnt expect this to work that easily tbh

void addstamp(int time){ for(tst*t=&tstmps; t; t=t->next){
	if(time==t->at) return;
	if(time>t->at && (!t->next || time<t->next->at)){
		t->next=new tst(time,t->next); return; }}}

struct Frame{ int time; Q a; Frame*next;
	Frame(int t=0,Q a=Q()):time(t),next(0),a(a){} ~Frame(){if(next)delete next;}
	Frame(Lisp l,int t):next(0){
		time=parseint(l.getstr(l[t]));
		a.x=parseint(l.getstr(l[t+1]));
		a.y=parseint(l.getstr(l[t+2]));
		a.z=parseint(l.getstr(l[t+3]));
	}
} cam; // current timestamp and view orientation

Q cast(int x,int y){ // from screen space to globe surface XXX use moar!
	float a=(x-width/2.)/scale, b=(y-height/2.)/scale, d=hypotf(a,b);
	if(d>1) d=1/d, a*=d, b*=d, d=1;
	return Q(sqrtf(1-d*d),a,b).rot(cam.a.inv());}

void line(Framework*w,_color col,Q a,Q b){ // helper for drawing lines
	w->line(a.x>0&&b.x>0?col:((col>>1)&0x777),
		a.y*scale+width/2, a.z*scale+height/2,
		b.y*scale+width/2, b.z*scale+height/2);}

int spd(Frame*a,Frame*b){ // helper for speed measurement, doesnt work
	const float s=1000; // scaling factor, since wed prefer ints
	return (Q(1,0,0).rot(euler(a->a)).dot(Q(1,0,0).rot(euler(b->a)))*-.5+.5)
		*s/(b->time-a->time);
}

// land[] contains all continents. done here so i dont have to deal w/
//  declaration-definition distinctions.
struct Continent; vector<Continent*>land; int current=-1; // index into land[]

struct Continent{ vector<Q>verts; Frame*frames; _color col; char*name;
	Q rot, prot[2]; // cached current and relative rotation to parents
	int parent[2]; // lifespan determined by frames[]
	char drawstate; // cached whether we copy rot from parent
	int speed; // cached w/ rot
	_color dcol; // what color to draw as (eg if following parent)
	enum tstate{none,processing,done}state; // used to figure out how to update

	Continent():rot(Q(1,0,0,0)),frames(0),col(0x8f0),name(0),state(none)
		{parent[0]=parent[1]=-1;}
	~Continent(){delete frames;}

	Continent(Lisp l,int i):Continent(){
		name=strdup(l.getstr(l[i++])); // parse name
		char*cs=l.getstr(l[i++]); // parse color (three-digit hex)
		col=
			((cs[0]-(cs[0]>'@'?'a'-10:'0'))<<8) |
			((cs[1]-(cs[1]>'@'?'a'-10:'0'))<<4) |
			(cs[2]-(cs[2]>'@'?'a'-10:'0'));
		for(int t=l[i++];l[t];t++) // parse verts
			verts.push_back(coord_quat(
				parseint(l.getstr(l[l[t]])),
				parseint(l.getstr(l[l[t]+1]))));
		// parse frames (potentially containing parental info)
		Frame f, *prev=&f; int s=l[i];
		if(l[s]>MAX_LISP_MEM) parent[0]=parseint(l.getstr(l[s++]));
		for(int t=s;l[t];t++){
			if(l[t]>MAX_LISP_MEM) {parent[1]=parseint(l.getstr(l[t])); break;}
			prev=prev->next=new Frame(l,l[t]);
		}frames=f.next; f.next=0;
	}

	void finalize(){
		// sets start/end offset relative to parent. has to happen after parent
		//  has been loaded, so its done separately from the constructor
		// XXX currently trying to figure out why one of them jumps...
		printf("dbg:  proc %s\n",name);
		if(state==done) return; // already did this one
		if(state==processing) // cycle breaker
			{printf("err: circular dependency during continent load!"); return;}
		state=processing;
		printf("dbg: proc %s\n",name);
		if(parent[0]!=-1){
			land[parent[0]]->finalize();
			Frame*t=land[parent[0]]->getframe(frames->time);
			prot[0]=euler(t->a).inv()*euler(frames->a);
			printf("  p0:\n");
			euler(frames->a).printw();
			euler(t->a).printw();
			prot[0].printw();
		}if(parent[1]!=-1){
			Frame*t=frames; while(t->next) t=t->next;
			land[parent[1]]->finalize();
			Frame*t2=land[parent[1]]->getframe(t->time);
			prot[1]=euler(t2->a).inv()*euler(t->a);
			printf("  p1:\n");
			euler(frames->a).printw();
			euler(t->a).printw();
			prot[1].printw();
		}state=done;
	}

	void save(FILE*f){ // no load() bc constructor takes care of that
		fprintf(f,"(%s %03x\n\t(",name?name:"Continent",col);
		for(Q q:verts){ q=quat_coord(q);
			fprintf(f,"(%.3f %.3f) ",q.x,q.y);
		}fprintf(f,")\n\t(");
		if(parent[0]!=-1) fprintf(f,"%i ",parent[0]);
		for(Frame*t=frames;t;t=t->next)
			fprintf(f,"(%i %.3f %.3f %.3f) ",
				t->time,t->a.x,t->a.y,t->a.z);
		if(parent[1]!=-1) fprintf(f,"%i",parent[1]);
		fprintf(f,")\n)\n\n");
	}

	void update(int ctime=-1){ // caches new rotation at timestamp
		if(ctime==-1) ctime=cam.time;
		if(state==done) return; // already processed
		if(state==processing)
			{printf("err: circular dependency in update!\n"); return;}
		state=processing;

		Frame*a=0, *b=0;
		for(Frame*f=frames;f;f=f->next){
			if(f->time<=ctime) a=f;
			if(f->time==ctime)      break;
			if(f->time>=ctime){b=f; break;}
		}if(!a) drawstate=1, speed=0, rot=euler(frames->a);
		else if(b){ drawstate=0;
			float t=(float)(ctime-a->time)/(b->time-a->time);
			speed=spd(a,b);
			rot=euler(a->a+(b->a-a->a)*t);
		}else{
			if(b=a->next) // if we are *exactly* on the frame
				speed=spd(a,b);
			else speed=0, drawstate=2;
			rot=euler(a->a);
		}
		// if we depend on parent, figure that out as well
		dcol=col;
		if(drawstate==1 && parent[0]!=-1){
			land[parent[0]]->update();
			speed=land[parent[0]]->speed;
			rot=land[parent[0]]->rot*prot[0];
			dcol=land[parent[0]]->col;
		}if(drawstate==2 && parent[1]!=-1){
			land[parent[1]]->update();
			speed=land[parent[1]]->speed;
			rot=land[parent[1]]->rot*prot[1];
			dcol=land[parent[1]]->col;
		}state=done;
	}

	void putframe(Frame*tmov){ // inserts new frame
		Frame*a=0, *b=0;
		for(Frame*f=frames;f;f=f->next){
			if(f->time<=tmov->time)  a=f;
			if(f->time==tmov->time)       break;
			if(f->time>=tmov->time) {b=f; break;}
		}if(!a) {tmov->next=frames; frames=tmov; return;}
		if(b) {tmov->next=b; a->next=tmov; return;}
		if(a->time==tmov->time) {a->a=tmov->a; delete tmov; return;}
		a->next=tmov;
	}

	Frame*getframe(int ctime){ // returns copy of frame
		Frame*a=0, *b=0;
		for(Frame*f=frames;f;f=f->next){
			if(f->time<=ctime)  a=f;
			if(f->time==ctime)       break;
			if(f->time>=ctime) {b=f; break;}
		}if(!a)return new Frame();
		if(b){
			float t=(float)(ctime-a->time)/(b->time-a->time);
			return new Frame(ctime,a->a+(b->a-a->a)*t);
		}return new Frame(ctime,a->a);
	}

	void draw(Framework*w,int ind=0){ // have a guess what this does
		Q rot=cam.a*this->rot, t; _color tcol=dcol;

		// front and back rendered separately
		int front=0, back=verts.size()*2;
		int*pts=(int*)malloc(back*sizeof(int));
		for(int i=0;i<verts.size();i++){
			t=verts[i].rot(rot);
			if(t.x>0){
				pts[front++]=t.y*scale+width/2;
				pts[front++]=t.z*scale+height/2;
			}else{
				pts[--back]=t.z*scale+height/2;
				pts[--back]=t.y*scale+width/2;
			}
		}
		tess(w, ind, tcol, pts      , front/2);
		tess(w,   0, tcol, pts+front, verts.size()-front/2);
		free(pts);

		// draw outline
		// XXX could save some cycles here by not recalculating all of the above?
		Q first=verts[0].rot(rot), prev=first, cur;
		tcol=(tcol>>1)&0x777;
		for(int i=0;i<verts.size();i++){
			cur=verts[i].rot(rot);
			line(w,tcol,prev,cur); prev=cur;
		}line(w,tcol,first,cur);
	}

	void tooltip(Framework*w){ // draws some extra doodads on selected landmass
		// as above
		Q rot=cam.a*this->rot, t; _color tcol=dcol;

		// draw pole
		t=Q(1,0,0).rot(rot);
		w->circf(tcol, t.y*scale+width/2, t.z*scale+height/2, 8);
		tcol=(tcol>>1)&0x777;
		w->circ (tcol, t.y*scale+width/2, t.z*scale+height/2, 8);
		if(name) // XXX make sure it has one?
			w->ps(name, t.y*scale+width/2, t.z*scale+height/2, 0xfff, 0xf000, 1);
		w->pi(speed, t.y*scale+width/2, t.z*scale+height/2+8, 0xfff, 0xf000, 1);

		// draw inline
		const float ss=.95;
		Q first=verts[0].rot(rot)*ss, prev=first, cur;
		for(int i=1;i<verts.size();i++){
			cur=verts[i].rot(rot)*ss;
			line(w,tcol,prev,cur); prev=cur;
			//w->pis(i,cur.y*scale+width/2+5,cur.z*scale+height/2+5);
		}line(w,tcol,first,cur);

		// draw keyframes
		for(Frame*f=frames;f;f=f->next){
			t=Q(1,0,0).rot(cam.a*euler(f->a));
			w->circ(tcol, t.y*scale+width/2, t.z*scale+height/2, 3);
			if(f!=frames) line(w,tcol,prev,t); prev=t;
		}
	}

};

int sqdst(int x,int y){return x*x+y*y;} // poor mans hypotf()
int dot(int ax,int ay,int bx,int by){return ax*bx+ay*by;} // dot product
void linedist(int ax,int ay,int bx,int by,int cx,int cy,int&rd,int&rx,int&ry){
	// figures out the point on a line closest to some point.
	// AB is a line segment, C a pt. rd is distance
	float dot_a  = dot(bx-ax,by-ay,cx-ax,cy-ay);
	if(dot_a<0){ rx=ax, ry=ay, rd=hypotf(ax-cx,ay-cy); return;}

	float dot_b  = dot(ax-bx,ay-by,cx-bx,cy-by);
	if(dot_b<0){ rx=bx, ry=by, rd=hypotf(bx-cx,by-cy); return;}

	float ang = 1/hypotf(ax-bx,ay-by); // length of segment
	dot_a=dot_a*ang, dot_b=dot_b*ang;
	ang=dot_a/(dot_a+dot_b); // interpolator
	//float ang=dot_a/(dot_a+dot_b)/hypotf(ax-bx,ay-by); // y u no work?
	rx = ax+ang*(bx-ax), ry = ay+ang*(by-ay);
	rd = hypotf(rx-cx,ry-cy);
}

void split(){ // rifts a continent along pts[]
	// tseg tells us which coast segment the rift starts at
	// first pt guaranteed to be on coast, will have to cast last one

	// adapted from below
	int cur=-1;{ // tells us which line segment we split to (ie tseg[1])
	int cx=pts[pts.size()-2], cy=pts[pts.size()-1];
	int n=land[current]->verts.size(), dist=10000,tdist, x,y, tx,ty;
	Q rot=cam.a*land[current]->rot, a, b;
	for(int i=0;i<n;i++){
		a=land[current]->verts[i].rot(rot);
		b=land[current]->verts[(i+1)%n].rot(rot);
		linedist(
			a.y*scale+width/2, a.z*scale+height/2,
			b.y*scale+width/2, b.z*scale+height/2,
			cx, cy, tdist,tx,ty);
		if(tdist<dist){dist=tdist,x=tx,y=ty,cur=i;}
	}pts[pts.size()-2]=x, pts[pts.size()-1]=y;}

	// cast new points to continent
	vector<Q>verts;
	for(int i=0;i<pts.size();i+=2)
		verts.push_back(cast(pts[i],pts[i+1]).rot(land[current]->rot.inv()));
	pts.clear(); // dont need them after this

	// make sure orientation is correct
	if(cur<tseg){
		int t=cur; cur=tseg; tseg=t;
		vector<Q>nverts; t=verts.size();
		for(int i=0;i<t;i++) {nverts.push_back(verts.back()); verts.pop_back();}
		verts=nverts;
	}cur++, tseg++; // not sure why this is needed tbh...

	// make first coastline
	vector<Q>coast_a;
	for(int i=0;i<tseg;i++) coast_a.push_back(land[current]->verts[i]);
	for(int i=0;i<verts.size();i++) coast_a.push_back(verts[i]);
	for(int i=cur;i<land[current]->verts.size();i++)
		coast_a.push_back(land[current]->verts[i]);

	// reverse new coast
	{int t=verts.size(); vector<Q>nverts;
	for(int i=0;i<t;i++) {nverts.push_back(verts.back()); verts.pop_back();}
	verts=nverts;}

	// make second coastline (in 'verts', since we dont otherwise need it)
	for(int i=tseg;i<cur;i++) verts.push_back(land[current]->verts[i]);

	land[current]->verts=coast_a; // adjust old coastline

	// make new continent XXX can prolly be done better...
	Continent*c=new Continent(); c->verts=verts;
	c->name = strdup(land[current]->name);
	c->parent[0] = land[current]->parent[0];
	c->parent[1] = land[current]->parent[1];

	// copy frames (bit hacky?)
	Frame*cf=c->frames=new Frame();
	c->frames[0]=*(land[current]->frames); c->frames->next=0;
	for(Frame*t=land[current]->frames->next;t;t=t->next){
		cf->next=new Frame();
		cf->next[0]=*t;
		cf->next->next=0;
		cf=cf->next;
	}

	// finalize new continent
	c->finalize(); c->update(); addstamp(cam.time);
	land.push_back(c); current=land.size()-1;
}

void makenew(){ // coast data in pts[]
	if(pts.size()<6) {pts.clear(); return;}
	Q s=Q(), t; vector<Q>verts; float a,b; // lat-long. s caches center
	for(int i=0; i<pts.size(); i+=2){
		t=cast(pts[i],pts[i+1]);
		verts.push_back(t); s+=t;
	}pts.clear(); // XXX the following do not account for signs
	s.w=0; s=s.norm(); a=asinf(s.z)*M_1_PI*.5;
	s.z=0; s=s.norm(); b=asinf(s.y)*M_1_PI*-.5;
	s=Q(-a,0,1,0).R() * Q(-b,0,0,1).R(); // inverse rotation, to recenter verts
	Continent*c=new Continent();
	c->name=strdup("Continent"); // ffs
	for(Q t:verts) c->verts.push_back(t.rot(s));
	c->frames=new Frame(cam.time,Q(0,a,b));
	c->update(); addstamp(cam.time);
	land.push_back(c); current=land.size()-1;
}

int pick(int mx,int my){return zbuf[my*width+mx]-1;} // neat!

void update(){static int t=-1; // make sure all cached rotations are correct
	if(t!=cam.time){ t=cam.time;
		for(Continent*c:land) c->state=(Continent::tstate)0; // reset
		for(Continent*c:land) c->update(); // now figures out order automagically
	}
}

void save(){
	FILE*f=fopen("savefile","w");
	for(Continent*c:land) c->save(f);
	fclose(f); printf("file saved\n");
}

void load(const char*fname){
	Lisp l(fname); if(!l[2]) {printf("could not load <%s>\n",fname); return;}
	land.clear(); current=0;
	tstmps=tst(0,0); // init timeline XXX does this delete previous?
	// XXX the following seems a bit repetitive, no?
	for(int i=2;l[i];i++) land.push_back(new Continent(l,l[i]));
	for(Continent*c:land) c->finalize();
	for(Continent*c:land) c->update();
	for(Continent*c:land) for(Frame*f=c->frames;f;f=f->next) addstamp(f->time);
	printf("file loaded\n");
}



// ---

// stuff for the speed graph. instead of using the data in the landmasses, we
//  cache the speeds in a separate structure.
// XXX uh... yeah maybe dont do that? 's pretty stupid, just track it in the
//  continents themselves...

struct ltstmp{int t,s; ltstmp*next;};

struct landtime{Continent*c; ltstmp*spd; landtime*next;} *ltlist=0;
int maxt, maxs; // maximum extent, cached so we can scale display properly

void maketime(){ // populates ltlist
	while(ltlist){ // if theres already a ltlist, delete it
		while(ltlist->spd) {ltstmp*t=ltlist->spd; ltlist->spd=t->next; delete t;}
		landtime*t=ltlist; ltlist=ltlist->next; delete t;
	}maxt=maxs=0;

	// foreach landmass
	//  foreach timestamp
	//   make ltstmp entry
	// foreach landmass
	//  figure out if we need parent stuff
	//  adjust ltstmp array
	// XXX do we *need* parent stuff?

	printf("dbg: recalculating timestamps...\n");
	for(Continent*c:land){
		landtime*t=new landtime();
		t->c=c; t->spd=0; t->next=ltlist; ltlist=t;
		ltstmp*tst=0;
		printf("continent <%s>:\n",c->name);
		for(Frame*f=c->frames;f;f=f->next){
			ltstmp*ts=new ltstmp();
			if(tst) tst->next=ts; else t->spd=ts; tst=ts;
			ts->t=f->time; ts->s=0; ts->next=0;
			if(f->next){
				ts->s=spd(f,f->next);
				printf("\t(% .3f % .3f % .3f % .3f : %.3f %i %i)\n",
					f->a.y, f->a.z, f->next->a.y, f->next->a.z,
					f->a.y*f->next->a.y+f->a.z*f->next->a.z,
					f->next->time-f->time,ts->s);
			}if(ts->t>maxt) maxt=ts->t; if(ts->s>maxs) maxs=ts->s;
		}putchar(10);
	}putchar(10);
}

// ---

// keeps track of current ui state
enum {Base, Split, Splitfirst, Draw, Move, Name, Join, Color, Time, Help}
	state=Base;

void doclick(Framework*w,bool loop){ // handles mouse inputs, manages pts[]
	int n=pts.size();
	if(loop){
		if(n==2) w->circ(0xfff,pts[0],pts[1],5);
		if(n>2) w->line(0xfff,pts[0],pts[1],pts[n-2],pts[n-1]);
		if(n>4) for(int i=0;i<n-2;i+=2)
			w->line(0xfff,pts[i],pts[i+1],pts[i+2],pts[i+3]);
	}else{
		if(n) w->circ(0xfff,pts[0],pts[1],5);
		if(n>2) for(int i=0;i<n-2;i+=2)
			w->line(0xfff,pts[i],pts[i+1],pts[i+2],pts[i+3]);
	}

	if(w->mouse.lp) {pts.push_back(w->mouse.x); pts.push_back(w->mouse.y);}
	if(w->keyp(SDL_SCANCODE_BACKSPACE)){
		if(n) pts.pop_back(), pts.pop_back();
		if(!pts.size()) state=Base;
	}if(w->keyp(SDL_SCANCODE_TAB)) {pts.clear(); state=Base;}
}

bool userfunc(Framework*w){
	{static bool quit=0;
	if(w->keyp(SDL_SCANCODE_ESCAPE)) quit=1;
	if(quit){ w->ps(
			"   Are you sure you want to quit?\n"
			"Enter to confirm, Backspace to abort\n"
			"       (no 'any' key detected)",
			w->_sx/16-17, w->_sy/16-1);
		if(w->keyp(SDL_SCANCODE_RETURN)) return 0;
		if(w->keyp(SDL_SCANCODE_BACKSPACE)) quit=0;
		return 1;}}

	// if we have an underlay img to draw, do so
	if(underlay.data){ const int stride=3;
		for(int x=max(0,width/2-scale);x<min(width,width/2+scale);x+=stride)
		for(int y=max(0,height/2-scale);y<min(height,height/2+scale);y+=stride)
		if(sqdst(x-width/2,y-height/2)<scale*scale){ Q q=cast(x,y);
			float lon=atan2f(q.x,q.y)*M_1_PI*-.5, lat=asinf(q.z)*M_1_PI+.5;
			_color col=underlay.at(lon*underlay.sx,lat*underlay.sy);
			for(int sx=0;sx<stride;sx++) for(int sy=0;sy<stride;sy++)
				w->p(col,x+sx,y+sy);
	}}else w->circf(0x111,width/2,height/2,scale); // globe outline

	{Q t=Q(0,0,-1).rot(cam.a); // equator indicator
	w->circf(t.x>0?0x00f:0x008,t.y*scale+width/2,t.z*scale+height/2,5);
	t=Q(0,0,1).rot(cam.a);
	w->circf(t.x>0?0x00f:0x008,t.y*scale+width/2,t.z*scale+height/2,5);
	t=Q(1,0,0);Q r=Q(1.0/12,0,0,1).R(),d;
	for(int i=0;i<12;i++){d=t.rot(cam.a);t=t.rot(r);
		w->circf(d.x>0?0xfff:0x888,d.y*scale+width/2,d.z*scale+height/2,2);}}

	// draw continents
	clear(); // depth buffer
	{int i=1; for(Continent*c:land) c->draw(w,i++);}
	if(land.size() && current!=-1) land[current]->tooltip(w);

	// draw continent 'title' XXX fuckssake, mate, just use sprintf!
	{int i=5+w->pi(cam.time,5,2); i+=w->ps(" Ga",i,2);
	if(current==-1) i+=w->ps(" (none)",i,2);
	else{ char buf[100];
		sprintf(buf," <%s> %imR/Ga",land[current]->name,land[current]->speed);
		i+=w->ps(buf,i,2);
	}}

	// draw timeline
	{int at=0; tst*t=&tstmps;
	for(int i=0; i<(width-80)/4; i++){
		if(t && t->at==i){ w->p(0xf0,40+i*4,8); t=t->next;}
		else w->p(0x8,40+i*4,8);
	}if(land.size() && current!=-1)
		for(Frame*t=land[current]->frames;t;t=t->next)
			w->p(0xff0,40+t->time*4,10);
	w->p(0xff,40+cam.time*4,6);}

	// display gimbal tooltip
	{const int gx=16,gy=16,gs=12;
	Q x=Q(1,0,0).rot(cam.a), y=Q(0,1,0).rot(cam.a), z=Q(0,0,1).rot(cam.a);
	w->circ(0x444,gx,gy,gs); w->p(0xfff,gx,gy);
	w->p(x.x<0?0x700:0xf00, x.y*gs+gx, x.z*gs+gy);
	w->p(y.x<0?0x070:0x0f0, y.y*gs+gx, y.z*gs+gy);
	w->p(z.x<0?0x007:0x00f, z.y*gs+gx, z.z*gs+gy);}

	static Frame*tmov=0; // stores Move data until we confirm or abort

	const int ttpx=1, ttpy=5; // tooltip position

	switch(state){
	case Base:{w->ps("Press 'H' for help",ttpx,ttpy);
		// handle movement in space + time
		{float inc=w->keyh(SDL_SCANCODE_LSHIFT)?.001:.01;
		Q pitch=Q(inc,0,1,0).R(), yaw=Q(inc,0,0,1).R(), a=Q(1,0,0,0), b=a;
		if(w->keyh(SDL_SCANCODE_W)) a=pitch;
		if(w->keyh(SDL_SCANCODE_S)) a=pitch.inv();
		if(w->keyh(SDL_SCANCODE_D)) b=yaw;
		if(w->keyh(SDL_SCANCODE_A)) b=yaw.inv();
		cam.a = a*cam.a*b; // quaternion application order can get weird
		if(Q(0,0,1).rot(cam.a).z<0) cam.a=a.inv()*cam.a; }

		if(w->keyh(SDL_SCANCODE_MINUS)) scale/=1.1;
		if(w->keyh(SDL_SCANCODE_EQUALS)) scale*=1.1;
		scale = max(height*.2,min(scale,height*2));
		if(w->keyp(SDL_SCANCODE_SPACE)) cam.a=Q(1,0,0,0), scale=height*.55;

		if(w->keyh(SDL_SCANCODE_Q)) cam.time--;
		if(w->keyh(SDL_SCANCODE_E)) cam.time++;
		if(w->keyp(SDL_SCANCODE_LEFTBRACKET)) cam.time--;
		if(w->keyp(SDL_SCANCODE_RIGHTBRACKET)) cam.time++;
		if(cam.time<0) cam.time=0; update();

		// handle continent selection
		if(w->mouse.lp) current=pick(w->mouse.x,w->mouse.y);

		// handle state transitions
		if(w->keyp(SDL_SCANCODE_O)) save();
		if(w->keyp(SDL_SCANCODE_L)) load("savefile");
		if(w->keyp(SDL_SCANCODE_H)) state=Help;
		if(w->keyp(SDL_SCANCODE_G)) state=Draw;
		if(w->keyp(SDL_SCANCODE_T)) maketime(), state=Time;
		if(land.size()){ // these are only valid if we *have* continents
			if(w->keyp(SDL_SCANCODE_C)) state=Color;
			if(w->keyp(SDL_SCANCODE_R)) state=Splitfirst;
			if(w->keyp(SDL_SCANCODE_M)) // cache, in case we abort
				tmov=land[current]->getframe(cam.time), state=Move;
			if(w->keyp(SDL_SCANCODE_N)) state=Name;
		}break;}

	case Splitfirst:{w->ps("Split coast",ttpx,ttpy);
		// for the first segment, we also gotta figure out which coast segment the
		//  rift starts on. hence a separate state.
		if(w->mouse.lp){ // using linedist to figure out nearest coastline
			int n=land[current]->verts.size(), dist=10000,tdist, x,y, tx,ty;
			Q rot=cam.a*land[current]->rot; tseg=-1;
			for(int i=0;i<n;i++){
				Q a=land[current]->verts[i].rot(rot);
				Q b=land[current]->verts[(i+1)%n].rot(rot);
				linedist(
					a.y*scale+width/2, a.z*scale+height/2,
					b.y*scale+width/2, b.z*scale+height/2,
					w->mouse.x, w->mouse.y, tdist,tx,ty);
				if(tdist<dist) {dist=tdist, x=tx, y=ty, tseg=i;}
			}pts.push_back(x); pts.push_back(y); state=Split;}
		if(w->keyp(SDL_SCANCODE_BACKSPACE)) state=Base;
		break;}

	case Split:{w->ps("Split continent",ttpx,ttpy); doclick(w,0);
		if(w->keyp(SDL_SCANCODE_RETURN)) {split(); state=Base;} break;}
	case Draw:{w->ps("New continent",ttpx,ttpy); doclick(w,1);
		if(w->keyp(SDL_SCANCODE_RETURN)) {makenew(); state=Base;} break;}

	case Move:{w->ps("Move continent",ttpx,ttpy);
		// we remap the keybindings used for camera in the Base state
		float inc=w->keyh(SDL_SCANCODE_LSHIFT)?.001:.01;
		if(w->keyh(SDL_SCANCODE_Q)) tmov->a.x+=inc;
		if(w->keyh(SDL_SCANCODE_E)) tmov->a.x-=inc;
		if(w->keyh(SDL_SCANCODE_S)) tmov->a.y+=inc;
		if(w->keyh(SDL_SCANCODE_W)) tmov->a.y-=inc;
		if(w->keyh(SDL_SCANCODE_A)) tmov->a.z+=inc;
		if(w->keyh(SDL_SCANCODE_D)) tmov->a.z-=inc;
		land[current]->rot=euler(tmov->a); // fortunately, lat-long commute
		for(Continent*c:land) c->update(); // so the preview looks right
		if(w->keyp(SDL_SCANCODE_BACKSPACE)){ // abort. recalculate cached rotation
			//land[current]->update_0(cam.time);
			//land[current]->update_1();
			land[current]->update();
			state=Base;
		}if(w->keyp(SDL_SCANCODE_RETURN))
			{land[current]->putframe(tmov); addstamp(cam.time); state=Base;}
		break;}

	case Join:{w->ps("Select parent",ttpx,ttpy);
		if(w->mouse.lp){
			int parent=pick(w->mouse.x,w->mouse.y);
			// XXX now what?
			// figure out relative position, insert frame for that, add link?
		}if(w->keyp(SDL_SCANCODE_BACKSPACE) || w->keyp(SDL_SCANCODE_RETURN))
			state=Base;
		break;}

	case Name:{w->ps("Type new continent name:",ttpx,ttpy);
		static char*buf=0; static int at=0;
		if(!buf){
			buf=(char*)malloc(100);
			if(land[current]->name){
				buf=strcpy(buf,land[current]->name);
				at=strlen(buf);
			}
		}

		char a=w->keyh(SDL_SCANCODE_LSHIFT)?'A':'a';
		for(int i=SDL_SCANCODE_A; i<SDL_SCANCODE_Z; i++)
			if(w->keyp(i)) buf[at++]=a+i-SDL_SCANCODE_A;
		if(w->keyp(SDL_SCANCODE_BACKSPACE) && at) buf[--at]=0;
		buf[at]=0;

		w->ps(buf,ttpx+1,ttpy+1); w->pc(0x16,ttpx+at+1,ttpy+1);

		if(w->keyp(SDL_SCANCODE_RETURN)){
			free(land[current]->name);
			land[current]->name=strdup(buf);
			free(buf); buf=0; at=0; state=Base;
		}return 1;} // no screenshot option

	case Color:{w->ps("Pick new color",ttpx,ttpy);
		_color col=land[current]->col; // weird bitfuckery for color picks
		for(int i=0;i<16;i++)w->pc(0xdb,4+i,4,(i<<8)|(col&0x0ff));
		for(int i=0;i<16;i++)w->pc(0xdb,4+i,6,(i<<4)|(col&0xf0f));
		for(int i=0;i<16;i++)w->pc(0xdb,4+i,8,(i<<0)|(col&0xff0));
		w->pc('X',4+(col>>8),4,col^0x0ff,0xf000);
		w->pc('X',4+((col>>4)&15),6,col^0xf0f,0xf000);
		w->pc('X',4+(col&15),8,col^0xff0,0xf000);

		if(w->mouse.lp){ // somewhat less weird bitfuckery for handling mouse
			int x=w->mouse.x>>3, y=w->mouse.y>>3;
			x-=4; if(x>16) break;
			if(y==4)col=(col&0x0ff)|(x<<8);
			if(y==6)col=(col&0xf0f)|(x<<4);
			if(y==8)col=(col&0xff0)|(x<<0);
		}land[current]->col=col;

		if(w->keyp(SDL_SCANCODE_BACKSPACE) || w->keyp(SDL_SCANCODE_RETURN))
			state=Base;
		break;}

	case Time:{w->ps("Speed graph",ttpx,ttpy);
		for(int i=0;i<width*height;i++)
			// i must say, i do NOT like the new alpha channel...
			w->newmap[i]=((w->newmap[i]>>1)&0x777)|0xf000;
		if(maxs!=0 && maxt!=0){
			float sx=(width-20)*1./maxt, sy=(height-40)*1./maxs; // scaling factors
			w->line(0xfff, 10, height-20, width-10, height-20);
			w->line(0xfff, cam.time*sx+10, 20, cam.time*sx+10, height-20);
			for(landtime*l=ltlist;l;l=l->next){ _color col=l->c->col;
				for(ltstmp*t0=l->spd,*t1;t0;t0=t1) if(t1=t0->next){
					w->line(col, sx*t0->t+15, height-sy*t0->s-20, sx*t1->t+5,  height-sy*t0->s-20);
					if(t1->next)
					w->line(col, sx*t1->t+5,  height-sy*t0->s-20, sx*t1->t+15, height-sy*t1->s-20);
					if(l->c==land[current]){
					w->pis(t0->s, sx*t0->t+17, height-sy*t0->s-28, 0xfff, 0xf000);
					w->line(col, sx*t0->t+17, height-sy*t0->s-22, sx*t1->t+7,  height-sy*t0->s-22);
					if(t1->next)
					w->line(col, sx*t1->t+7,  height-sy*t0->s-22, sx*t1->t+17, height-sy*t1->s-22);
					}
				}
			}
		}else w->ps("no time data",width/16-10,height/16);

		if(w->keyp(SDL_SCANCODE_BACKSPACE) || w->keyp(SDL_SCANCODE_RETURN)
			|| w->keyp(SDL_SCANCODE_T)) state=Base;
		break;}

	case Help:{w->ps(helptext,4,10); // long-ass string
		if(w->keyp(SDL_SCANCODE_RETURN)
			|| w->keyp(SDL_SCANCODE_BACKSPACE)
			|| w->keyp(SDL_SCANCODE_H))
				state=Base;
		break;}
	}if(w->keyp(SDL_SCANCODE_P)) w->snap(); return 1;
}

int main(int argc,char**argv){ cam.a=Q(1,0,0,0);
	if(argc>1) load(argv[1]); else load("savefile");
	if(argc>2) underlay=img(argv[2]);
	Framework w(width,height,1); w.run(userfunc); return 0;}
