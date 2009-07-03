/* ******************************************************************
   Arquivos  de exemplo  para  o desenvolvimento  de  um algoritmo  de
   branch-and-cut  usando  o XPRESS.   Este  branch-and-cut resolve  o
   problema  da mochila  0-1 e  usa  como cortes  as desigualdades  de
   cobertura simples (cover inequalities) da  forma x(c) < |C| -1 onde
   C � um conjunto de itens formando uma cobertura minimal.

   Autor: Cid Carvalho de Souza 
          Instituto de Computa��o - UNICAMP - Brazil

   Data: segundo semestre de 2003

   Arquivo: global.h
   Descri��o: declara��es globais.  
 * *************************************************************** */

/* includes e estruturas globais */
#define HGLOBAIS 1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* Include para  o uso  da biblioteca  assert.  Para  compilar  com os
   asserts, comentar a linha abaixo. */
/*#define NDEBUG 1*/
#include <assert.h>

/*  Includes para  uso do  XPRESS: neste  caso sup�e-se  que  ele est�
   instalado no diret�rio /opt/xpressmp-2008 */
#include "xprs.h"
#define XPDIR "/opt/xpressmp-2008/" 

/* definic�es do usu�rio */
typedef enum{false,true} Boolean;
#define TRUE true;
#define FALSE false;
#define True true;
#define False false;

/* Constantes para definir estrat�gias de corte */
#define MAX_NUM_CORTES 100   /* limite maximo de cortes */
#define MAX_ITER_SEP 10   /* numero maximo de iteracoes de corte por nodo */
#define EPSILON 0.000001 /* uma toler�ncia usada em alguns c�lculos com doubles*/

/* limitando o tempo de CPU */
#define MAX_CPU_TIME -1800  /* colocar negativo ! (veja manual do XPRESS) */

