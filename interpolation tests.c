#include <stdio.h>
#define A 100 //v_max
#define B  1 //v_min
#define N 25
#define SMOOTHSTEP(x) ((x) * (x) * (3 - 2 * (x)))

void interpolL();
void interpolS();
void interpolSmulti(int);
void interpolSq();


int main(){
    //interpolL();
    //interpolS();
    //interpolSmulti(2);
    //interpolSq();


	return 0;
}

void interpolL(){
	float v,x,vmax;
	int i;

	for (i = 0; i <N; i++)
	{
		v = 1.0*i / N;
		//v=SMOOTHSTEP(v);
  		x = (A * v) + (B * (1 - v));
  		//x=100*v;
  		printf("Doy %d steps a %.2f velocidad.\n", 1,x);
	}

    printf("Doy %d steps a %.2f velocidad.\n", 50,x);

    vmax = x;
	for (i = 0; i < N; i++)
	{
		v = 1.0*i/N;
  		x = (B * v) + (vmax * (1 - v));
  		printf("Doy %d steps a %.2f velocidad.\n", 1,x);
	}
}

void interpolS(){
	float v,x,vmax;
	int i;

	for (i = 0; i <N; i++)
	{
		v = 1.0*i / N;
		v=SMOOTHSTEP(v);
  		x = (A * v) + (B * (1 - v)); //VELOCIDAD INICAL != 0
  		//x=100*v;
  		printf("Doy %d steps a %.2f velocidad.\n", 1,x);
	}

    printf("Doy %d steps a %.2f velocidad.\n", 50,x);

    vmax = x;
	for (i = 0; i < N; i++)
	{
		v = 1.0*i/N;
		v = SMOOTHSTEP(v);
  		x = (1 * v) + (vmax * (1 - v));
  		printf("Doy %d steps a %.2f velocidad.\n", 1,x);
	}
}

void interpolSmulti(int n){
	float v,x,vmax;
	int i,j;

	for (i = 0; i <N; i++)
	{
		v = 1.0*i / N;
        for (j=0; j<n;j++){
            v=SMOOTHSTEP(v);
        }
  		x = (A * v) + (B * (1 - v)); //VELOCIDAD INICAL != 0
  		//x=100*v;
  		printf("Doy %d steps a %.2f velocidad.\n", 1,x);
	}

    printf("Doy %d steps a %.2f velocidad.\n", 50,x);

    vmax = x;
	for (i = 0; i < N; i++)
	{
		v = 1.0*i/N;
		for (j=0; j<n;j++){
            v=SMOOTHSTEP(v);
        }
  		x = (1 * v) + (vmax * (1 - v));
  		printf("Doy %d steps a %.2f velocidad.\n", 1,x);
	}
}

void interpolSq(){
	float v,x,vmax;
	int i,j;

	for (i = 0; i <N; i++)
	{
		v = 1.0*i / N;
        v=v*v;
  		x = (A * v) + (B * (1 - v)); //VELOCIDAD INICAL != 0
  		//x=100*v;
  		printf("Doy %d steps a %.2f velocidad.\n", 1,x);
	}

    printf("Doy %d steps a %.2f velocidad.\n", 50,x);

    vmax = x;
	for (i = 0; i < N; i++)
	{
		v = 1.0*i/N;
		v = v*v;
  		x = (B * v) + (x * (1 - v));
  		printf("Doy %d steps a %.2f velocidad.\n", 1,x);
	}
}




