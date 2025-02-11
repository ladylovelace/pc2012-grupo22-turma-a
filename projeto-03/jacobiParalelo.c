//
//  jabociParalelo.c
//  tr3conc
//
//  Created by Leticia Silva on 5/23/12.
//  Copyright (c) 2012 __MyCompanyName__. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/time.h>
#include <mpi.h>
//#include <omp.h>

#define TRUE 1
#define FALSE 0

typedef float **MATRIZ;
typedef float cast;

typedef struct bloco{
    int ini;
    int fim;
    cast **A;
    cast *X_old;
    cast *X_new;
    cast *B;
    
} Bloco;

void dividirPelaDiagonal( int *ordem, cast **matriz, cast *diagonal, cast *t_indep ) {
    
	int i, j;
	cast valor;
    
	/* Dividir cada equaÃ§Ã£o pelo correspondente elemento da diagonal principal */
	//printf("COEFICIENTES DA MATRIZ DIVIDIDO PELA DIAGONAL PRINCIPAL\n");
	for( i = 0; i < *ordem; i++ ) {
        
		t_indep[i] = t_indep[i] / diagonal[i];
        
		for( j = 0; j < *ordem; j ++ ) {
            
			matriz[i][j] = matriz[i][j] / diagonal[i];
            
			//printf("%f   ", matriz[i][j] );
            
		}
		//printf("\n");
	}
    
	//printf("\n");
	/*Teste do critÃ©rio de convergÃªncia: para este caso foi utilizado o critÃ©rio das linhas */
	for( i = 0; i < *ordem; i++ ) {
        
		valor = 0.0;
        
		for( j = 0; j < *ordem; j++ ) {
            
			if( i != j && valor < 1 ) {
                
				valor += matriz[i][j];
                
			} else if( valor > 1 ) {
                
				printf( "O processo de Jacobi-Richardson aplicado ao sistema dado NAO convergerah\n" );
				exit( 1 );
			}
		}
		//printf("Somatorio da linha: %d = %f\n", i, valor);
	}
}

void iteracao( cast **matriz, cast *t_indep, cast *x_old, cast *x_new, cast *erro, int *filaAval, int *maxIter, int *ordem, cast *diagonal ) {
    
	int k = 0, j, i;
	cast thisErro = 1.0;
	cast valorAprox = 0.0;
	int flag = FALSE;
    
	while( k < *maxIter && !flag ) {
        
		for( i = 0; i < *ordem ; i++ ) {
            
			for( j = 0; j < *ordem; j++ ) {
                
				if( i != j ) {
                    
					/* X = -( L* + R* )x + b* */
					//printf( "(%f x %f) + ", -1 * matriz[i][j], x_old[j] );
					x_new[i] += ( -1 * matriz[i][j] ) * x_old[j];
				}
			}
            
			/* Soma do termo independente */
			x_new[i] += t_indep[i];
			//printf( " %f =  %f\n", t_indep[i], x_new[i] );
		}
        
		//printf("x_old: %f -- x_new: %f\n", x_old[*filaAval - 1], x_new[*filaAval - 1]);
		thisErro = ( fabs( x_new[*filaAval - 1] - x_old[*filaAval - 1] ) ) / fabs( x_new[*filaAval - 1] );
		//printf( "ERRO: %f\n", thisErro );
        
		if( thisErro > *erro && k < *maxIter - 1) {
            
			for( j = 0; j < *ordem; j ++ ) {
                
				x_old[j] = x_new[j];
				x_new[j] = 0;
				//printf("Vetor[%d]: %f\n", j, vetor[j]);
			}
            
		} else
			flag = TRUE;
        
		k++;
	}
    
	for( i = 0; i < *ordem; i++ )
		valorAprox += matriz[*filaAval][i] * x_new[i];
    
    valorAprox = valorAprox * diagonal[*filaAval];
    
	//printf( "\n--------------------------------------------\n" );
	//printf( "Iterations: %d\n", k );
	//printf( "RowTest: %d => [%f] =? %f\n", *filaAval, valorAprox, ( t_indep[*filaAval] * diagonal[*filaAval] ) );
	//if( flag )
    //printf( "CondiÃ§Ã£o de parada por ERRO\n" );
	//else
    //printf( "Condicao de parada por ITERACAO\n");
	//printf( "--------------------------------------------\n\n" );
}


int main (int argc,char**argv){
    FILE *file;
    char processorName[MPI_MAX_PROCESSOR_NAME];
    int ordem, maxIter, filaAval, coef;
    int i, j,k;
    int numSlave, qtdeLinhas;
    cast erro;
    MATRIZ matriz;
    cast *t_indep, *x_old, *x_new, *diagonal;
    struct timeval iniTempo;
    struct timeval fimTempo;
    MPI_Status status;
    int nroProc, myRank, nameLen;           
    
    MPI_Init ( &argc, &argv );
    
    MPI_Comm_size ( MPI_COMM_WORLD, &nroProc );
    MPI_Comm_rank ( MPI_COMM_WORLD, &myRank );
    MPI_Get_processor_name( processorName, &nameLen );
    
    
    /* Inicia o cálculo do tempo */
    gettimeofday( &iniTempo, NULL );
    double tS = iniTempo.tv_sec + ( iniTempo.tv_usec / 1000000.0 );
    
    if( ( file = fopen( argv[1], "r" ) ) == NULL ) {
        
        printf( "ERRO: na abertura do arquivo\n" );
        MPI_Finalize();
        exit( 1 );
        
    } else {
            fscanf( file, "%d", &ordem );
            //printf( "Ordem: %d\n", ordem );
            fscanf( file, "%d", &filaAval );
            //printf( "Fila avaliacao: %d\n", filaAval );
            fscanf( file, "%f", &erro );
            //printf( "Ordem: %f\n", erro );
            fscanf( file, "%d", &maxIter );
            //printf( "MaxIteracao: %d\n", maxIter );
            
            /* Alocação de memória para os valores da diagonal principal */
            diagonal = ( cast* ) malloc ( ( ordem ) * sizeof( cast ) );
            
            /* Alocação de memória das linhas da matriz */
            matriz = ( cast** ) malloc ( ( ordem ) * sizeof( cast* ) ); 
            
            /* Alocação de memória das colunas e o preechimento da matriz */
            for( i = 0; i < ordem; i++ ) {
                
                matriz[i] = ( cast* ) malloc( ( ordem + 1 ) * sizeof( cast ) );
                
                for( j = 0; j < ordem; j++ ) { 
                    
                    fscanf( file, "%f ", &matriz[i][j] );
                    
                    if( i == j ) 
                        diagonal[i] = matriz[i][j];
                }
            }
            x_old = ( cast* ) malloc ( ( ordem ) * sizeof( cast ) );
            x_new = ( cast* ) malloc ( ( ordem ) * sizeof( cast ) );
            
            /* Alocação de memória para os termos independentes da equação */
            t_indep = ( cast* ) malloc ( ( ordem ) * sizeof( cast ) );
            
            /* Preechimento do termo independente */
            for( i = 0; i < ordem; i++ ) {
                
                fscanf( file, "%f", &t_indep[i] );
                x_old[i] = 0.0;
                
            }
            
            /* Conferencia da matriz */
            printf("\nCOEFICIENTES DA MATRIZ\n");
            for( i = 0; i < ordem; i++ ) {
                
                for( j = 0; j < ordem; j ++ )
                    
                    printf("%f   ", matriz[i][j] );
                
                printf("\n");
            }
            
            /* Conferencia do vetor */
            printf("\nVALORES DO VETOR\n");
            for( i = 0; i < ordem; i++ )
                printf( "%f ", t_indep[i] );
            
            printf("\n\n");
        }
        //dividirPelaDiagonal( &ordem, matriz, diagonal, t_indep ); /// DESCOMENTAR ISSO!
        
		/** Separar a matriz no número de processos escravos **/
        numSlave = 2; // número igual ao número de slaves
        qtdeLinhas = ordem /numSlave;
        printf("\n");
        printf("\n");
        printf("número de processos: %d, ordem: %d e pedaço: %d", numSlave, ordem, qtdeLinhas);
        printf("\n");
        Bloco decomposto[numSlave];
        int inicio = 0;
        for(i=1; i<= numSlave; i++){
            if (i<=(ordem%numSlave)){
                printf("comeca em: %d\n", inicio);
                decomposto[i-1].ini = inicio;
                inicio += qtdeLinhas;
                printf("termina em: %d\n", inicio);
                decomposto[i-1].fim = inicio;
                
                decomposto[i-1].A = matriz;
                decomposto[i-1].X_old = x_old;
                decomposto[i-1].X_new = x_new;
                decomposto[i-1].B = t_indep;
                
                //printf("> %f<\n", decomposto[0].A[0][0]);
                for( j = decomposto[i-1].ini; j <= decomposto[i-1].fim; j++ ) {
                    for( k = 0; k < ordem; k++ ){
                        printf("%f   ", decomposto[i-1].A[j][k] );
                    }
                    printf("\n");
                }
                /*printf("termos B: \n");
                 for( k = 0; k < ordem; k++ ){
                 printf("%f ", decomposto[i-1].B[k]);
                 }*/
                printf("\n");
            }
            else{
                printf("comeca em: %d\n", inicio);
                decomposto[i-1].ini = inicio;
                inicio += (qtdeLinhas -1);
                printf("termina em: %d\n", inicio);
                decomposto[i-1].fim = inicio;
                
                decomposto[i-1].A = matriz;
                decomposto[i-1].X_old = x_old;
                decomposto[i-1].X_new = x_new;
                decomposto[i-1].B = t_indep;
                
                for( j = decomposto[i-1].ini; j <= decomposto[i-1].fim; j++ ) {
                    for( k = 0; k < ordem; k++ ){
                        printf("%f   ", decomposto[i-1].A[j][k] );
                    }
                    printf("\n");
                }
                printf("\n");
            }
            inicio ++;
        }
        
		iteracao( matriz, t_indep, x_old, x_new, &erro, &filaAval, &maxIter , &ordem, diagonal );
    }
    /* Termina o cálculo do tempo */
    gettimeofday( &fimTempo, NULL );
    double tE = fimTempo.tv_sec + ( fimTempo.tv_usec / 1000000.0 );
    MPI_Finalize();
    printf( "Tempo de execução do processo: %d eh de %.3lf segundos\n\n", myRank, tE - tS );
    
    /* Desalocação de memória */
	for( i = 0; i < ordem; i++ )
		free( matriz[i] );
    
	free( matriz );
	free( diagonal );
	free( x_old );
	free( x_new );
    
	fclose( file );
    return EXIT_SUCCESS;
}