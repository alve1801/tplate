/* spc's quaternion class
 Copyright (C) 2017-2026 Ave Tealeaf

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

struct Q{float w,x,y,z;Q(Lisp,int);
	Q(float x=0,float y=0,float z=0):w(0),x(x),y(y),z(z){};
	Q(float w,float x,float y,float z):w(w),x(x),y(y),z(z){};
	void printw(){printf("% .3f % .3f % .3f % .3f : %.3f\n",this->w,this->x,this->y,this->z,this->abs());}
	void print(){printf("% .2f % .2f % .2f\n",this->x,this->y,this->z);}
	Q inv(){return Q(this->w,-this->x,-this->y,-this->z);} // Q operator-(){} ?
	//float dot(Q a){return (this->w*a.w +this->x*a.x +this->y*a.y +this->z*a.z);}
	float dot(Q a){return this->x*a.x +this->y*a.y +this->z*a.z;}
	float sqabs(){return this->dot(*this);}
	float abs(){return sqrt(sqabs());}
	Q norm(){float a=this->abs();if(a==0)return Q();return *this/a;}
	Q wnorm(){return Q(x,y,z).norm();}
	Q operator+(Q a){return Q(this->w+a.w,this->x+a.x,this->y+a.y,this->z+a.z);}
	Q operator+=(Q a){return(*this=*this+a);}
	Q operator-(Q a){return Q(this->w-a.w,this->x-a.x,this->y-a.y,this->z-a.z);}
	Q operator-=(Q a){return(*this=*this-a);}
	Q operator*(Q a){/*B cross A*/Q b=*this;return Q(
		a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,
		a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,
		a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,
		a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w);}
	Q operator*=(Q a){return(*this=*this*a);}
	Q operator*(float a){return Q(this->w*a,this->x*a,this->y*a,this->z*a);}
	Q operator*=(float a){return(*this=*this*a);}
	Q operator/(float a){a=1/a;return (*this)*a;}
	Q operator/=(float a){return(*this=*this/a);}
	float operator%(Q a){return dot(a);}
	//bool operator==(Q a){return this->w==a.w&&this->x==a.x&&this->y==a.y&&this->z==a.z;}
	bool operator==(Q a){return (*this-a).sqabs()<.0001;} // or similar
	bool operator!=(Q a){return !(*this==a);}
	Q R(){
		float a=x*x+y*y+z*z,s,c;
		if(a<.01)return Q(1,0,0,0);
		sincosf(w*M_PI,&s,&c);s/=sqrt(a);
		return Q(c,s*x,s*y,s*z);
	}Q rot(Q q){/*q=q.norm();*/return q*(*this)*q.inv();} // aka conj qaq'
};

Q::Q(Lisp l,int i){
	w=parseint(l.getstr(l[i]));i++;
	x=parseint(l.getstr(l[i]));i++;
	y=parseint(l.getstr(l[i]));i++;
	z=parseint(l.getstr(l[i]));i++;
}

// conversion routines from/to latlong
// for coords, first arg is polar angle (southward), second is phi (eastward)
// ough, math gets weird. can we simplify?

Q quat_coord(Q t){
	Q res;
	res.x=acosf(t.z);
	res.y=atan2f(t.x,t.y);
	return res;
}

Q coord_quat(float a,float b){
	Q res; float s,c;
	sincosf(b,&s,&c);
	res.x=s, res.y=c;
	sincosf(a,&s,&c);
	res.x*=s, res.y*=s, res.z=c;
	return res;
}

// casts from lat-long-yaw to quats proper
Q euler(Q a){return Q(a.z,0,0,1).R()*Q(a.y,0,1,0).R()*Q(a.x,1,0,0).R();}
