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
//const int width=960, height=720;
const int width=1280, height=960;
float scale=height*.55;

#include"parse.h" // from spc, for savefiles
#include"tess.h" // rasterizer
#include"q.h" // from spc, for trig stuff
#include"helptext.h" // long-ass string istg

Q pts[100]; int ptsize=0; // user input
struct tst{int at; tst*next; // gui indicator for keyframes (cached)
	tst(int a,tst*n):at(a),next(n){} ~tst(){ if(next) delete next; next=0;}
} tstmps(0,0); // XXX remove? n put where?

img underlay; // didnt expect this to work that easily tbh

void addstamp(int time){ for(tst*t=&tstmps; t; t=t->next){
	if(time==t->at) return;
	if(time>t->at && (!t->next || time<t->next->at)){
		t->next=new tst(time,t->next); return; }}}

struct Frame{ int t, s; Q a; Frame*next;
	// time, (cached) speed, and latlongyaw
	Frame(int t=0,Q a=Q()):t(t),next(0),s(0),a(a){}
	~Frame() {if(next) delete next;}
	Frame(Lisp l,int t):Frame(){
		this->t=parseint(l.getstr(l[t]));
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
	if(!b) return 0;
	if(a->t==b->t) {printf("ERR: frame overlap\n"); return s;}
	return (Q(1,0,0).rot(euler(a->a)).dot(Q(1,0,0).rot(euler(b->a)))*-.5+.5)
		*s/(b->t-a->t);
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

	Continent():rot(Q(1,0,0,0)),frames(0),col(0x8f0),name(0),state(none),speed(0)
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

	void finalize(bool ovr=0){
		// sets start/end offset relative to parent. has to happen after parent
		//  has been loaded, so its done separately from the constructor
		// XXX currently trying to figure out why one of them jumps...
		if(ovr) state=none;
		if(state==done) return; // already did this one
		if(state==processing) // cycle breaker
			{printf("err: circular dependency during continent load!"); return;}
		state=processing;
		if(parent[0]!=-1){
			land[parent[0]]->finalize();
			Q t=land[parent[0]]->getframe(frames->t);
			prot[0]=euler(t).inv()*euler(frames->a);
		}if(parent[1]!=-1){
			Frame*t=frames; while(t->next) t=t->next;
			land[parent[1]]->finalize();
			Q t2=land[parent[1]]->getframe(t->t);
			prot[1]=euler(t2).inv()*euler(t->a);
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
				t->t,t->a.x,t->a.y,t->a.z);
		if(parent[1]!=-1) fprintf(f,"%i",parent[1]);
		fprintf(f,")\n)\n\n");
	}

	// cache speed in timestamps
	void time() {for(Frame*f=frames;f;f=f->next) f->s=spd(f,f->next);}

	void update(bool ovr=0){ // caches new rotation at timestamp
		if(ovr) state=none; // updates regardless of state
		if(state==done) return; // already processed
		if(state==processing)
			{printf("err: circular dependency in update!\n"); return;}
		state=processing; dcol=col; Frame*a=0, *b=0;
		for(Frame*f=frames;f;f=f->next){
			if(f->t<=cam.t) a=f;
			if(f->t==cam.t)      break;
			if(f->t>=cam.t){b=f; break;}
		}if(!a) drawstate=1, speed=0, rot=euler(frames->a);
		else if(b){ drawstate=0;
			float t=(float)(cam.t-a->t)/(b->t-a->t);
			speed=spd(a,b);
			rot=euler(a->a+(b->a-a->a)*t); // still find it weird that this works
		}else{
			if(b=a->next) speed=spd(a,b); // if we are *exactly* on the frame
			else speed=0, drawstate=2;
			rot=euler(a->a);
		} // if we depend on parent, figure that out as well
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
		// XXX only used when manually moving continent. do in-place?
		Frame*a=0, *b=0;
		for(Frame*f=frames;f;f=f->next){
			if(f->t<=tmov->t)  a=f;
			if(f->t==tmov->t)       break;
			if(f->t>=tmov->t) {b=f; break;}
		}if(!a) {tmov->next=frames; frames=tmov; return;}
		if(b) {tmov->next=b; a->next=tmov; return;}
		if(a->t==tmov->t) {a->a=tmov->a; delete tmov; return;}
		a->next=tmov;
	}

	Q getframe(int ctime){ // returns position at time
		Frame*a=0, *b=0;
		for(Frame*f=frames;f;f=f->next){
			if(f->t<=ctime)  a=f;
			if(f->t==ctime)       break;
			if(f->t>=ctime) {b=f; break;}
		}if(!a) return Q();
		if(b){
			float t=(float)(ctime-a->t)/(b->t-a->t);
			return a->a+(b->a-a->a)*t;
		}return a->a;
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

		// draw outline XXX cache form above? XXX barely visible...
		Q first=verts[0].rot(rot), prev=first, cur; tcol=(tcol>>1)&0x777;
		for(int i=0;i<verts.size();i++)
			{cur=verts[i].rot(rot); line(w,tcol,prev,cur); prev=cur;}
		line(w,tcol,first,cur);
	}

	void tooltip(Framework*w){ // draws some extra doodads on selected landmass
		Q rot=cam.a*this->rot, t; _color tcol=(dcol>>1)&0x777;
		t=Q(1,0,0).rot(rot); // draw pole
		w->circf(col, t.y*scale+width/2, t.z*scale+height/2, 8);
		w->circ ((col>>1)&0x777, t.y*scale+width/2, t.z*scale+height/2, 8);
		if(name) // XXX make sure it always has one?
			w->ps(name, t.y*scale+width/2, t.z*scale+height/2, 0xfff, 0xf000, 1);
		w->pi(speed, t.y*scale+width/2, t.z*scale+height/2+8, 0xfff, 0xf000, 1);
		const float ss=.95; // draw inline
		Q first=verts[0].rot(rot)*ss, prev=first, cur;
		for(int i=1;i<verts.size();i++){
			cur=verts[i].rot(rot)*ss;
			line(w,tcol,prev,cur); prev=cur;
			w->pis(i,cur.y*scale+width/2+5,cur.z*scale+height/2+5);
		}line(w,tcol,first,cur);
		for(Frame*f=frames;f;f=f->next){ // draw keyframes
			t=Q(1,0,0).rot(cam.a*euler(f->a));
			w->circ(col, t.y*scale+width/2, t.z*scale+height/2, 3);
			if(f!=frames) line(w,col,prev,t); prev=t;
	}}
};

Q qldst(Q a,Q b,Q c,float&d){ // projects pt to lineseg
	float na=(c-a).dot(b-a); if(na<0) {d=(c-a).abs(); return a;}
	float nb=(c-b).dot(a-b); if(nb<0) {d=(c-b).abs(); return b;}
	Q ret=a+(b-a)*(c-a).abs()/(b-a).abs(); d=(ret-c).abs(); return ret;
}

void split(){ // rifts a continent along pts[]
	int s1=-1, s2=-1; // which segments we split on
	Q q1, q2, qt; // and *where* we split on them
	float d1=1000, d2=1000, d; // aaand distances to them, for the min alg
	#define n land[current]->verts.size()
	for(int i=0;i<ptsize;i++) pts[i]=pts[i].rot(land[current]->rot.inv());
	for(int i=0;i<n;i++){
		Q qa=land[current]->verts[i], qb=land[current]->verts[(i+1)%n];
		qt=qldst(qa,qb,pts[0],d); if(d<d1) {d1=d; q1=qt; s1=i;}
		qt=qldst(qa,qb,pts[ptsize-1],d); if(d<d2) {d2=d; q2=qt; s2=i;}
	}pts[0]=q1, pts[ptsize-1]=q2;
	bool rev=s1>s2; if(rev) {int t=s1; s1=s2; s2=t;} s1++, s2++;
	vector<Q>l1;
	for(int i=0;i<s1;i++) l1.push_back(land[current]->verts[i]);
	if(rev) for(int i=ptsize-1;i>-1;i--) l1.push_back(pts[i]);
	else for(int i=0;i<ptsize;i++) l1.push_back(pts[i]);
	for(int i=s2;i<n;i++) l1.push_back(land[current]->verts[i]);
	vector<Q>l2;
	for(int i=s1;i<s2;i++) l2.push_back(land[current]->verts[i]);
	if(rev) for(int i=0;i<ptsize;i++) l2.push_back(pts[i]);
	else for(int i=ptsize-1;i>-1;i--) l2.push_back(pts[i]);
	#undef n
	ptsize=0; land[current]->verts=l1;
	Continent*c=new Continent(); c->verts=l2;
	c->name = strdup(land[current]->name);
	// XXX new continent only gets one frame, n its parent is the other one
	c->parent[0] = land[current]->parent[0];
	c->parent[1] = land[current]->parent[1];
	Frame*cf=c->frames=new Frame(); // bit hacky?
	c->frames[0]=*(land[current]->frames); c->frames->next=0;
	for(Frame*t=land[current]->frames->next;t;t=t->next)
		{cf->next=new Frame(); cf->next[0]=*t; cf->next->next=0; cf=cf->next;}
	c->finalize(1); c->update(1); addstamp(cam.t);
	land.push_back(c); current=land.size()-1;
}

void makenew(){ // coast data in pts[]
	if(ptsize<2) {ptsize=0; return;}
	Q s=Q(); float a,b;
	for(int i=0;i<ptsize;i++) s+=pts[i];
	// XXX the following do not (properly) account for signs
	s.w=0; s=s.norm(); a=asinf(s.z)*M_1_PI*.5;
	s.z=0; s=s.norm(); b=asinf(s.y)*M_1_PI*-.5;
	s=Q(-a,0,1,0).R() * Q(-b,0,0,1).R(); // inverse rotation, to recenter verts
	Continent*c=new Continent();
	c->name=strdup("Continent"); // ffs
	for(int i=0;i<ptsize;i++) c->verts.push_back(pts[i].rot(s)); ptsize=0;
	c->frames=new Frame(cam.t,Q(0,a,b));
	c->finalize(1); c->update(1); addstamp(cam.t);
	land.push_back(c); current=land.size()-1;
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
	for(Continent*c:land) c->state=(Continent::tstate)0;
	for(Continent*c:land) c->update();
	for(Continent*c:land) for(Frame*f=c->frames;f;f=f->next) addstamp(f->t);
	printf("file loaded\n");
}



// ---

// keeps track of current ui state
enum {Base, Split, Draw, Move, Name, Join, Color, Time, Help} state=Base;

void scast(Q a,int&x,int&y) // cast pt to screen
	{a=a.rot(cam.a)*scale; x=a.y+width/2; y=a.z+height/2;}

void doclick(Framework*w,bool loop){ // handles mouse inputs, manages pts[]
	int p0,p1,p2,p3; // dunno how i feel abt these names...
	if(ptsize==1){ // XXX or just always do it?
		scast(pts[0],p0,p1);
		w->circ(0xfff,p0,p1,5);
	}
	for(int i=0;i<ptsize-1;i++){
		scast(pts[i],p0,p1);
		scast(pts[i+1],p2,p3);
		w->line(0xfff, p0,p1,p2,p3);
	}
	if(ptsize>2&&loop){
		scast(pts[0],p0,p1);
		scast(pts[ptsize-1],p2,p3);
		w->line(0xfff, p0,p1,p2,p3);
	}

	if(w->mouse.lp) pts[ptsize++]=cast(w->mouse.x,w->mouse.y);
	//if(w->keyp(SDL_SCANCODE_BACKSPACE)) if(!--ptsize) state=Base;
	//if(w->keyp(SDL_SCANCODE_TAB)) ptsize=0, state=Base;

	if(w->keyp(KBKSP)) if(!--ptsize) state=Base;
	if(w->keyp(KTAB)) ptsize=0, state=Base;
}

void domove(Framework*w){ // handles qweasd + zoom
	float inc=w->keyh(KSHIFT)?.001:.01;
	Q pitch=Q(inc,0,1,0).R(), yaw=Q(inc,0,0,1).R(), a=Q(1,0,0,0), b=a;
	if(w->keyh(KW)) a=pitch;
	if(w->keyh(KS)) a=pitch.inv();
	if(w->keyh(KD)) b=yaw;
	if(w->keyh(KA)) b=yaw.inv();
	cam.a = a*cam.a*b; // quaternion application order can get weird
	if(Q(0,0,1).rot(cam.a).z<0) cam.a=a.inv()*cam.a;

	if(w->keyh(KMINUS)) scale/=1.1;
	if(w->keyh(KPLUS)) scale*=1.1;
	scale = max(height*.2,min(scale,height*2));
	if(w->keyp(KSPC)) cam.a=Q(1,0,0,0), scale=height*.55;

	int t=cam.t;
	if(w->keyh(KQ)) cam.t--;
	if(w->keyh(KE)) cam.t++;
	if(w->keyp(KLBR)) cam.t--;
	if(w->keyp(KRBR)) cam.t++;
	if(cam.t<0) cam.t=0;
	if(cam.t!=t){
		for(Continent*c:land) c->state=(Continent::tstate)0;
		for(Continent*c:land) c->update();
	}

	// XXX not sure this really belongs here, but it makes sense for now
	if(w->mouse.lp) current=pick(w->mouse.x,w->mouse.y);
}

bool userfunc(Framework*w){
	{static bool quit=0;
	if(w->keyp(KESC)) quit=1;
	if(quit){ w->ps(
			"   Are you sure you want to quit?\n"
			"Enter to confirm, Backspace to abort\n"
			"       (no 'any' key detected)",
			w->_sx/16-17, w->_sy/16-1);
		if(w->keyp(KRET)) return 0;
		if(w->keyp(KBKSP)) quit=0;
		return 1;}}

	// if we have an underlay img to draw, do so
	if(underlay.data){ const int stride=3;
		#define sqdst(x,y) ((x)*(x)+(y)*(y)) // poor mans hypotf
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
	{int i=5+w->pi(cam.t,5,2); i+=w->ps(" Ga",i,2);
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
			w->p(0xff0,40+t->t*4,10);
	w->p(0xff,40+cam.t*4,6);}

	// display gimbal tooltip
	{const int gx=16,gy=16,gs=12;
	Q x=Q(1,0,0).rot(cam.a), y=Q(0,1,0).rot(cam.a), z=Q(0,0,1).rot(cam.a);
	w->circ(0x444,gx,gy,gs); w->p(0xfff,gx,gy);
	w->p(x.x<0?0x700:0xf00, x.y*gs+gx, x.z*gs+gy);
	w->p(y.x<0?0x070:0x0f0, y.y*gs+gx, y.z*gs+gy);
	w->p(z.x<0?0x007:0x00f, z.y*gs+gx, z.z*gs+gy);}

	static Q tmov; // stores Move data until we confirm or abort
	static int maxs, maxt; // stores maximum extent in time/speed

	const int ttpx=1, ttpy=5; // tooltip position

	switch(state){
	case Base:{w->ps("Press 'H' for help",ttpx,ttpy);
		domove(w); // move, scrub, reselect continent

		// handle state transitions
		if(w->keyp(KO)) save();
		if(w->keyp(KL)) load("savefile");
		if(w->keyp(KH)) state=Help;
		if(w->keyp(KG)) state=Draw;
		if(w->keyp(KT)) {maxs=maxt=0;
			for(Continent*c:land) {c->time();
				for(Frame*f=c->frames;f;f=f->next){
					if(f->s>maxs) maxs=f->s;
					if(f->t>maxt) maxt=f->t;
			}}state=Time;
		}
		if(land.size()){ // these are only valid if we *have* continents
			if(w->keyp(KC)) state=Color;
			if(w->keyp(KR)) state=Split;
			if(w->keyp(KJ)) state=Join;
			if(w->keyp(KM)) // cache, in case we abort
				tmov=land[current]->getframe(cam.t), state=Move;
			if(w->keyp(KN)) state=Name;
		}break;}

	case Split:{w->ps("Split continent",ttpx,ttpy); doclick(w,0);
		if(w->keyp(KRET)) {split(); state=Base;} break;}
	case Draw:{w->ps("New continent",ttpx,ttpy); doclick(w,1);
		if(w->keyp(KRET)) {makenew(); state=Base;} break;}

	case Move:{w->ps("Move continent",ttpx,ttpy);
		// we remap the keybindings used for camera in the Base state
		float inc=w->keyh(KSHIFT)?.001:.01;
		if(w->keyh(KQ)) tmov.x+=inc;
		if(w->keyh(KE)) tmov.x-=inc;
		if(w->keyh(KS)) tmov.y+=inc;
		if(w->keyh(KW)) tmov.y-=inc;
		if(w->keyh(KA)) tmov.z+=inc;
		if(w->keyh(KD)) tmov.z-=inc;
		land[current]->rot=euler(tmov); // fortunately, lat-long commute
		if(w->keyp(KBKSP)) // abort. recalculate cached rotation
			{land[current]->update(1); state=Base;}
		if(w->keyp(KRET)){ // confirm. insert as timestamp
			land[current]->putframe(new Frame(cam.t,tmov));
			addstamp(cam.t); state=Base;
		}break;}

	case Join:{w->ps("Select parent",ttpx,ttpy);
		if(w->mouse.lp){
			int p=pick(w->mouse.x,w->mouse.y); if(p==-1 || p==current) break;
			land[current]->parent[1]=p;
			Frame*t=new Frame(cam.t,land[current]->getframe(cam.t));
			land[current]->putframe(t); delete t->next; t->next=0;
			land[current]->finalize(1);
			state=Base;
		}if(w->keyp(KBKSP) || w->keyp(KRET)) state=Base; break;}

	case Name:{w->ps("Type new continent name:",ttpx,ttpy);
		static char*buf=0; static int at=0;
		if(!buf){
			buf=(char*)malloc(100);
			if(land[current]->name){
				buf=strcpy(buf,land[current]->name);
				at=strlen(buf);
			}
		}

		char a=w->keyh(KSHIFT)?'A':'a';
		for(int i=KA;i<KZ;i++) if(w->keyp(i)) buf[at++]=a+i-KA;
		// XXX space, dash, underline, numbers, etc
		if(w->keyp(KBKSP) && at) buf[--at]=0;
		buf[at]=0;

		w->ps(buf,ttpx+1,ttpy+1); w->pc(0x16,ttpx+at+1,ttpy+1);

		if(w->keyp(KRET)){
			free(land[current]->name);
			land[current]->name=strdup(buf);
			free(buf); buf=0; at=0; state=Base;
		}return 1;} // no screenshot option

	case Color:{w->ps("Pick new color",ttpx,ttpy);
		_color col=land[current]->col; // weird bitfuckery for color picks
		for(int i=0;i<16;i++) w->pc(0xdb,4+i,ttpy+2,(i<<8)|(col&0x0ff));
		for(int i=0;i<16;i++) w->pc(0xdb,4+i,ttpy+4,(i<<4)|(col&0xf0f));
		for(int i=0;i<16;i++) w->pc(0xdb,4+i,ttpy+6,(i<<0)|(col&0xff0));
		w->pc('X',4+(col>>8),      ttpy+2,col^0x0ff,0xf000);
		w->pc('X',4+((col>>4)&15), ttpy+4,col^0xf0f,0xf000);
		w->pc('X',4+(col&15),      ttpy+6,col^0xff0,0xf000);

		if(w->mouse.lp){ // somewhat less weird bitfuckery for handling mouse
			int x=w->mouse.x>>3, y=w->mouse.y>>3;
			x-=4; if(x>16) break; y-=ttpy;
			if(y==2)col=(col&0x0ff)|(x<<8);
			if(y==4)col=(col&0xf0f)|(x<<4);
			if(y==6)col=(col&0xff0)|(x<<0);
		}land[current]->dcol=land[current]->col=col;

		if(w->keyp(KBKSP) || w->keyp(KRET)) state=Base; break;}

	case Time:{w->ps("Speed graph",ttpx,ttpy);
		for(int i=0;i<width*height;i++)
			// i must say, i do NOT like the new alpha channel...
			w->newmap[i]=((w->newmap[i]>>1)&0x777)|0xf000;

		if(maxs==0 || maxt==0) {state=Base; break;}
		// XXX lines are upside down
		float mt=(width-40)/maxt, ms=(height-20)/maxs; Frame*g;
		w->line(0xfff, cam.t*mt+20, 20, cam.t*mt+20, height-20);
		for(Continent*c:land){ Frame*f=c->frames,*g;
			if(c->parent[0]!=-1) // draw first parent node
				w->circf(land[c->parent[0]]->col,f->t*mt+15,f->s*ms+10,5);
			while(g=f->next){
				w->line(c->col, f->t*mt+25, f->s*ms+10, g->t*mt+15, f->s*ms+10);
				if(g->next) // no conn on last line
				w->line(c->col, g->t*mt+15, f->s*ms+10, g->t*mt+25, g->s*ms+10);
				else if(c->parent[1]!=-1) // draw last parent node
					w->circf(land[c->parent[1]]->col,g->t*mt+25,f->s*ms+10,5);
				if(c==land[current]){ // offset
					w->pis(f->s, f->t*mt+32, f->s*ms+4, 0xfff, 0xf000);
					w->line(c->col, f->t*mt+28, f->s*ms+13, g->t*mt+12, f->s*ms+13);
					if(g->next)
					w->line(c->col, g->t*mt+12, f->s*ms+13, g->t*mt+28, g->s*ms+13);
				} f=g;
		}} domove(w); // also handles selection

		if(w->keyp(KBKSP) || w->keyp(KRET) || w->keyp(KT)) state=Base; break;}

	case Help:{w->ps(helptext,4,10); // long-ass string
		if(w->keyp(KBKSP) || w->keyp(KRET) || w->keyp(KH)) state=Base; break;}
	}if(w->keyp(KP)) w->snap(); return 1;
}

int main(int argc,char**argv){ cam.a=Q(1,0,0,0);
	if(argc>1) load(argv[1]); else load("savefile");
	if(argc>2) underlay=img(argv[2]);
	Framework w(width,height,1); w.run(userfunc); return 0;}

/*

noul/subducter glitches, and idk why.

might also wanna have a thingy for "continent does not exist at current time"
akin to parental stuff ig, but fucked if i knew how to implement it...
if no parent specified, continent does not exist if no frames have data for
 current time? idk that sounds like itd lead to a lot of trouble...
i mean, *implementing* it should be fairly easy (new drawtype, two extra vars
 for birth and death time), but *interfacing* w/ it user-side...
hm. well, we dont have to keep it in the keyframe list? would make the savefile
 a bit more confusing tho. how make use in ui?
actually. *where* does this cause issues? cos the more i think abt it, the more
 viable it seems, if we can figure out how to cover the edge cases...
- what happens if we depend on a parent, but the parent does not exist? would
 it spell trouble if we also disappear?

*/
