/* sexpr parser from spc, adapted somewhat
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

#ifndef LISP
#define LISP
#define MAX_LISP_MEM 0x2000
#define MAX_LISP_STR 0x4000
#include<stdio.h>

struct Lisp{int mem[MAX_LISP_MEM], mp, sp; char str[MAX_LISP_STR], lh;
	Lisp(const char*fname){ mp=1, sp=0;
		for(int i=0;i<MAX_LISP_MEM;i++) mem[i]=0;
		for(int i=0;i<MAX_LISP_STR;i++) str[i]=0;
		FILE*f=fopen(fname,"r");
		if(!f){printf("invalid savefile!\n");return;}
		get(f); parse(f); fclose(f); cdrcode();
	}char get(FILE*f){return lh=getc(f);}
	int parse(FILE*); void white(FILE*);
	void cdrcode(); void print(int);
	char*getstr(int x){return str+x-MAX_LISP_MEM;}
	int&operator[](int x){return mem[x];}
};

void Lisp::white(FILE*f){while(lh==' '||lh=='\t'||lh=='\n')get(f);}
int Lisp::parse(FILE*f){
	if(mp>=MAX_LISP_MEM || sp>=MAX_LISP_STR){
		printf("lisp out of memory!\n");return 0;}
	if(lh==')' || lh==EOF){get(f);white(f);return 0;}
	int ret=mp; mp+=2;
	if(lh=='(')get(f),mem[ret]=parse(f);
	else{mem[ret]=sp+MAX_LISP_MEM;
		while(lh!=' ' && lh!='\n' && lh!='\t' &&
			lh!='(' && lh!=')' && lh!=EOF && sp<MAX_LISP_STR)
			str[sp++]=lh,get(f);
		str[sp++]=0,white(f);
	}mem[ret+1]=parse(f);return ret;
}

void Lisp::cdrcode(){
	int newmem[MAX_LISP_MEM],np=1,proc=1;
	newmem[np++]=1;
	while(proc<np){
		int env=newmem[proc];
		if(env && env<MAX_LISP_MEM){
			newmem[proc]=np;
			while(env){
				newmem[np++]=mem[env];
				env=mem[env+1];
			}newmem[np++]=0;
		}proc++;
	}for(int i=0;i<np;i++)mem[i]=newmem[i];
}

float parseint(const char*s){
	float ret=0,prec=1; bool inv=0;
	if(*s=='-'){inv=1;s++;}
	while('0'<=*s&&*s<='9')ret=ret*10+*s++ -'0';
	if(*s++=='.')while('0'<=*s&&*s<='9')
		ret+=(*s++ -'0')*(prec/=10);
	if(inv)ret*=-1;return ret;
}

#endif
