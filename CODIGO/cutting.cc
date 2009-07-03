/* ******************************************************************
   Arquivos  de exemplo  para  o desenvolvimento  de  um algoritmo  de
   branch-and-cut  usando  o XPRESS.   Este  branch-and-cut resolve  o
   problema  da mochila  0-1 e  usa  como cortes  as desigualdades  de
   cobertura simples (cover inequalities) da  forma x(c) < |C| -1 onde
   C � um conjunto de itens formando uma cobertura minimal.

   Autor: Cid Carvalho de Souza 
          Instituto de Computa��o - UNICAMP - Brazil

   Data: segundo semestre de 2003

   Arquivo: princ.c
   Descri��o: arquivo com programas em C
 * *************************************************************** */

 
/* includes dos arquivos .h */
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>

#include "integer_program.H"
#include "cutting.H"
#include "definitions.H"
#include "xprs.h"

/* variaveis globais */

/* valor de retorno das rotinas do XPRESS */
int xpress_ret;   
/* status retornado por algumas rotinas do XPRESS */
int xpress_status;   
/* armazena a melhor solucao inteira encontrada */
double *xstar;   
/* armazena o valor da solucao otima */
double zstar;  
/* armazena as solucoes do LP e do ILP  */
double *x;    
/* - diz se usara ou nao a heuristica primal */
bool HEURISTICA_PRIMAL;
/* - diz se usara ou nao cortes */
bool BRANCH_AND_CUT;
/* - n� onde encontrou a melhor solucao inteira */
int NODE_BEST_INTEGER_SOL;
/* estrutura do XPRESS contendo o problema */
XPRSprob prob;   
/* contador do total de cortes por n� da arvore B&B */
int totcuts;  
/* contador do total de n�s explorados B&B */
int totnodes;  
/* contador do total de lacos de separacao de cortes por n� da arvore B&B */
int itersep;   
/* - profundidade maxima de um no da arvore de B&B aonde sera feita separacao */
int MAX_NODE_DEPTH_FOR_SEP;
/* - valor otimo da primeira relaxacao */
double objval_relax;
/* - valor da relaxacao linear no final do 1o n� */
double objval_node1;

/* declaracoes das callbacks */
int XPRS_CC Cortes(XPRSprob prob, void* data);
void XPRS_CC SalvaMelhorSol(XPRSprob prob, void *my_object);

/* rotinas auxiliares */
void errormsg(const char *sSubName,int nLineNo,int nErrCode);
void ImprimeSol(double *x);
void HeuristicaPrimal(int node);
void Mochila(double *c,int *w,int b,int n,int *x,double *val);

/* mensagem de uso do programa */
void showUsage() {
	printf ("Uso: knap <estrategia> <prof_max_para_corte> < <instancia> \n");
        printf ("- estrategia: string de \"0\"\'s e \"1\"\'s de tamanho 2.\n");
	printf ("  - 1a. posi��o � \"1\" se a heur�stica primal � usada. \n");
	printf ("  - 2a. posi��o � \"1\" se as minhas \"cover inequalities\" forem separadas.\n");
	printf ("    Nota: estrategia=\"00\" equivale a um Branch-and-Bound puro\n");
	printf ("- prof_max_para_corte: maior altura de um n� para aplicar cortes\n");
	printf ("  (default=1000000) \n");
	printf ("- instancia: nome do arquivo contendo a inst�ncia \n");
}

CuttingPlanes::CuttingPlanes(IntegerProgram &ip, bool hp, bool bnc) : ip(ip) {

    bool relaxed;

    // FIXME: colocar em IntegerProgram
    char probname[] = "stab";

    /* inicializa valores de variaveis globais */
    HEURISTICA_PRIMAL = hp; BRANCH_AND_CUT = bnc;
    totcuts=0; totnodes = 0; itersep=0; zstar=XPRS_MINUSINFINITY; 
    MAX_NODE_DEPTH_FOR_SEP = 1000000;
    NODE_BEST_INTEGER_SOL = -1;

    ip.getParam(n, m, &qrtype, &rhs, &obj, &mstart, &mrwind,
		&dmatval, &dlb, &dub, n, &qgtype, &mgcols, relaxed);

    /* incializac�o do XPRESS */
    xpress_ret=XPRSinit("");
    if (xpress_ret) errormsg("Main: Erro de inicializacao do XPRESS.\n",__LINE__,xpress_ret);

    /* "cria" o problema  */
    xpress_ret=XPRScreateprob(&prob);
    if (xpress_ret) errormsg("Main: Erro na initializacao do problema",__LINE__,xpress_ret);

    /* ======================================================================== */
    /* Atribui valores a v�rios parametros de controle do XPRESS                */
    /* ======================================================================== */

    /* limita o tempo de execucao */
    xpress_ret=XPRSsetintcontrol(prob,XPRS_MAXTIME,MAX_CPU_TIME);
    if (xpress_ret) errormsg("Main: Erro ao tentar setar o XPRS_MAXTIME.\n",__LINE__,xpress_ret);

    /* aloca  espa�o extra de  linhas para inser��o de  cortes. CUIDADO:
       Isto tem que ser feito ANTES (!) de carregar o problema ! */
    xpress_ret=XPRSsetintcontrol(prob,XPRS_EXTRAROWS,MAX_NUM_CORTES+1);
    if (xpress_ret) 
      errormsg("Main: Erro ao tentar setar o XPRS_EXTRAROWS.\n",__LINE__,xpress_ret);
  
    /*  aloca espa�o  extra  de  elementos n�o  nulos  para inser��o  de
	cortes. CUIDADO: Isto  tem que ser feito ANTES  (!) de carregar o
	problema ! */
    xpress_ret=XPRSsetintcontrol(prob,XPRS_EXTRAELEMS,n*n);
    if (xpress_ret) 
      errormsg("Main: Erro ao tentar setar o XPRS_EXTRAELEMS.",__LINE__,xpress_ret);
    
    xpress_ret=XPRSloadglobal(prob, probname, n, m,  qrtype, rhs, NULL, obj, mstart, NULL, 
			      mrwind, dmatval, dlb, dub, n, 0, qgtype, mgcols, NULL, NULL, NULL,  NULL, NULL);
    if (xpress_ret) errormsg("Main: Erro na carga do modelo.",__LINE__,xpress_ret);

    /* libera memoria dos vetores usado na carga do LP */
    free(qrtype);   free(rhs);    free(obj);      free(mstart);   free(mrwind);   free(dmatval); 
    free(dlb);      free(dub);    free(qgtype);   free(mgcols); 
    
    /* salva um arquivo ".lp" com o LP original */
    xpress_ret=XPRSwriteprob(prob,"LP","l");
    if (xpress_ret) 
      errormsg("Main: Erro na chamada da rotina XPRSwriteprob.\n",__LINE__,xpress_ret);
    
    /* Desabilita o PRESOLVE: o problema da mochila � muito f�cil para o XPRESS */
    xpress_ret=XPRSsetintcontrol(prob,XPRS_PRESOLVE,0);
    if (xpress_ret) errormsg("Main: Erro ao desabilitar o presolve.",__LINE__,xpress_ret);
  }
bool CuttingPlanes::solve() {
    /* impress�o para confer�ncia */
    if (HEURISTICA_PRIMAL) printf("*** Heur�stica Primal ser� usada\n");

    if (BRANCH_AND_CUT) { 

      printf("*** Algoritmo de branch-and-cut.\n");
      /*  aloca  espaco  para  as  estruturas que  serao  usadas  para
       * armazenar  os cortes  encontrados na separacao.   Para evitar
       * perda de tempo, estas  estruturas sao alocadas uma unica vez.
       * Para  isso, o  tamanho das mesmas  deve ser o  maior possivel
       *  para  comportar os  dados  obtidos  por  *qualquer* uma  das
       * rotinas  de separacao. No  caso a rotina de  separa��o poder�
       *  gerar at� um  "cover ineuqality"  por vez.  Ler no  manual a
       * descri��o da rotina XPRSaddcuts */
      qrtype=(char *)malloc(sizeof(char)); 
      mtype=(int *) malloc(sizeof(int));
      drhs=(double *)malloc(sizeof(double));
      mstart=(int *)malloc((n+1)*sizeof(int));
      mcols=(int *)malloc(n*sizeof(int));   /* cada corte tera no maximo n nao-zeros */
      dmatval=(double *)malloc(n*sizeof(double));

      /* callback indicando que sera feita separacao de cortes em cada
         n� da arvore de B&B */
      xpress_ret=XPRSsetcbcutmgr(prob,Cortes,NULL);

      if (xpress_ret) 
	errormsg("Main: Erro na chamada da rotina XPRSsetcbcutmgr.\n",__LINE__,xpress_ret);
      /*=====================================================================================*/
      /* RELEASE NOTE: The controls CPKEEPALLCUTS, CPMAXCUTS and CPMAXELEMS have been removed*/    
      /* Diz ao XPRESS que quer manter cortes no pool 
	 xpress_ret=XPRSsetintcontrol(prob,XPRS_CPKEEPALLCUTS,0); 
	 if (xpress_ret)   
         errormsg("Main: Erro ao tentar setar o XPRS_CPKEEPALLCUTS.\n",__LINE__,xpress_ret); */ 
      /*=====================================================================================*/
    } 
    else { 

      printf("*** Algoritmo de branch-and-bound puro.\n");

      qrtype=NULL;   mtype=NULL;   drhs=NULL;   mstart=NULL;   
      mcols=NULL;    dmatval=NULL;

      if (HEURISTICA_PRIMAL) {
	/*  callback indicando que sera feita separacao de cortes em
	    cada n� da  arvore de B&B.  Mas, neste  caso, a rotina
	    nao  faz   corte,  limitando-se  apenas   a  executar  a
	    heuristica primal. */
	xpress_ret=XPRSsetcbcutmgr(prob,Cortes,NULL);
	if (xpress_ret) 
	  errormsg("Main: rotina XPRSsetcbcutmgr.\n",__LINE__,xpress_ret);
      }
    }

    /* Desabilita a separacao de cortes do XPRESS. Mochila � muito f�cil para o XPRESS */
    xpress_ret=XPRSsetintcontrol(prob,XPRS_CUTSTRATEGY,0);
    if (xpress_ret) 
      errormsg("Main: Erro ao tentar setar o XPRS_CUTSTRATEGY.\n",__LINE__,xpress_ret);

    /* callback para salvar a melhor solucao inteira encontrada */
    xpress_ret=XPRSsetcbintsol(prob,SalvaMelhorSol,NULL);
    if (xpress_ret) 
      errormsg("Main: Erro na chamada da rotina XPRSsetcbintsol.\n",__LINE__,xpress_ret);
  
    /* aloca  espaco  para o  vetor  "x"  que  contera as  solucoes  das
     * relaxacoes e  de "xstar" que armazenara a  melhor solucao inteira
     * encontrada. */
    x=(double *)malloc(n*sizeof(double));
    xstar=(double *)malloc(n*sizeof(double));

    /* resolve o problema */
    xpress_ret=XPRSmaxim(prob,"g");
    if (xpress_ret) errormsg("Main: Erro na chamada da rotina XPRSmaxim.\n",__LINE__,xpress_ret);

    /* imprime a solucao otima ou a melhor solucao encontrada (se achou)  e o seu valor */
    xpress_ret=XPRSgetintattrib(prob,XPRS_MIPSTATUS,&xpress_status);
    if (xpress_ret) 
      errormsg("Main: Erro na chamada da rotina XPRSgetintatrib.\n",__LINE__,xpress_ret);

    if ((xpress_status==XPRS_MIP_OPTIMAL)  || 
	(xpress_status==XPRS_MIP_SOLUTION) ||
	(zstar > XPRS_MINUSINFINITY)){

      XPRSgetintattrib(prob,XPRS_MIPSOLNODE,&NODE_BEST_INTEGER_SOL);
      printf("\n");
      printf("- Valor da solucao otima =%12.6f \n",(double)(zstar));
      printf("- Variaveis otimas: (n�=%d)\n",NODE_BEST_INTEGER_SOL);

      if ( zstar == XPRS_MINUSINFINITY ) {
	xpress_ret=XPRSgetsol(prob,xstar,NULL,NULL,NULL);
	if (xpress_ret) 
	  errormsg("Main: Erro na chamada da rotina XPRSgetsol\n",__LINE__,xpress_ret);
      }
      ImprimeSol(xstar);

    } else  printf("Main: programa terminou sem achar solucao inteira !\n");

    /* impressao de estatisticas */
    printf("********************\n");
    printf("Estatisticas finais:\n");
    printf("********************\n");
    printf(".total de cortes inseridos ........ = %d\n",totcuts);
    printf(".valor da FO da primeira relaxa��o. = %.6f\n",objval_relax);
    printf(".valor da FO no n� raiz ........... = %.6f\n",objval_node1);

    xpress_ret=XPRSgetintattrib(prob,XPRS_NODES,&totnodes);
    if (xpress_ret) 
      errormsg("Main: Erro na chamada da rotina XPRSgetintatrib.\n",__LINE__,xpress_ret);

    printf(".total de n�s explorados .......... = %d\n",totnodes);
    printf(".n� da melhor solucao inteira ..... = %d\n",NODE_BEST_INTEGER_SOL);
    printf(".valor da melhor solucao inteira .. = %d\n",(int)(zstar+0.5));  
    /* somar 0.5 evita erros de arredondamento */

    /* verifica o valor do melhor_limitante_dual */
    xpress_ret=XPRSgetdblattrib(prob,XPRS_BESTBOUND,&melhor_limitante_dual);
    if (xpress_ret) 
      errormsg("Main: Erro na chamada de XPRSgetdblattrib.\n",__LINE__,xpress_ret);

    if (melhor_limitante_dual < zstar+EPSILON)
      melhor_limitante_dual=zstar; 
    printf(".melhor limitante dual ............ = %.6f\n",melhor_limitante_dual);

    /* libera a memoria usada pelo problema */
    /*
      xpress_ret=XPRSdestroyprob(prob);
      if (xpress_ret) 
      errormsg("Main: Erro na liberacao da memoria usada pelo problema.\n",__LINE__,xpress_ret);
    */

    /* libera toda memoria usada no programa */
    free(qrtype);
    free(mtype);
    free(drhs);
    free(mstart);
    free(mcols);
    free(dmatval);
    free(x);
    free(xstar);

    xpress_ret=XPRSfree();
    if (xpress_ret)
      errormsg("Main: Erro na liberacao de memoria final.\n",__LINE__,xpress_ret);
    printf("========================================\n");

    return true;
  }

/* =====================================================================
 * Rotina (callback) para salvar a  melhor solucao.  Roda para todo n�
 * onde acha solucao inteira.
 * =====================================================================
*/
void XPRS_CC SalvaMelhorSol(XPRSprob prob, void *my_object)
{
   int i, cols, peso_aux=0, node;
   double objval;
   bool viavel;

   /* pega o numero do n� corrente */
   xpress_ret=XPRSgetintattrib(prob,XPRS_NODES,&node);
   if (xpress_ret) 
     errormsg("SalvaMelhorSol: rotina XPRSgetintattrib.\n",__LINE__,xpress_ret);

   xpress_ret=XPRSgetintattrib(prob,XPRS_COLS,&cols);
   if (xpress_ret) 
       errormsg("SalvaMelhorSol: rotina XPRSgetintattrib\n",__LINE__,xpress_ret);

   xpress_ret=XPRSgetdblattrib(prob,XPRS_LPOBJVAL,&objval);
   if (xpress_ret) 
       errormsg("SalvaMelhorSol: rotina XPRSgetdblattrib\n",__LINE__,xpress_ret);

   xpress_ret=XPRSgetsol(prob,x,NULL,NULL,NULL);
   if (xpress_ret) 
     errormsg("SalvaMelhorSol: Erro na chamada da rotina XPRSgetsol\n",__LINE__,xpress_ret);

   /* testa se a solu��o � vi�vel */
   // for(i=0;i<cols;i++) peso_aux += x[i]*w[i];
   // viavel=(peso_aux <= W + EPSILON);

   /*
   printf("\n..Encontrada uma solu��o inteira (n�=%d): valor=%f, peso=%d,",
	  node,objval,peso_aux);
   if (viavel) printf(" viavel\n"); else printf(" inviavel\n");
   for(i=0;i<cols;i++) 
     if (x[i]>EPSILON) printf(" x[%3d] = %12.6f (w=%6d, c=%12.6f)\n",i,x[i],w[i],c[i]);
   */

   /* se a solucao tiver custo melhor que a melhor solucao disponivel entao salva */
   if ((objval > zstar-EPSILON) && viavel) {

     printf(".. atualizando melhor solu��o ...\n");
     for(i=0;i<cols;i++) xstar[i]=x[i];
     zstar=objval;
     
     /* informa xpress sobre novo incumbent */
     xpress_ret=XPRSsetdblcontrol(prob,XPRS_MIPABSCUTOFF,zstar+1.0-EPSILON);
     if (xpress_ret) 
       errormsg("SalvaMelhorSol: XPRSsetdblcontrol.\n",__LINE__,xpress_ret);
     
     NODE_BEST_INTEGER_SOL=node; 
     /* Impress�o para sa�da */
     printf("..Melhor solu��o inteira encontrada no n� %d, peso %d e custo %12.6f\n",
	    NODE_BEST_INTEGER_SOL,peso_aux,zstar);
     printf("..Solu��o encontrada: \n");
     ImprimeSol(x);
   }
   
   return;
}

/**********************************************************************************\
* Rotina para separacao de Cover inequalities. Roda em todo n�.
* Autor: Cid Carvalho de Souza 
* Data: segundo semestre de 2003
\**********************************************************************************/
int XPRS_CC Cortes(XPRSprob prob, void* data)
{
    // int encontrou, i, irhs, k, node, node_depth, solfile, ret;
    // int nLPStatus, nIntInf;

    // /*  variaveis para a separacao das cover inequalities */
    // int *peso, capacidade, *sol;
    // double *custo, val, lpobjval, ajuste_val;


    // /* se for B&B puro e n�o usar Heur�stica Primal, n�o faz nada */
    // if ( (!BRANCH_AND_CUT) && (!HEURISTICA_PRIMAL)) return 0;

    // /* Recupera a  status do LP e o  n�mero de inviabilidades inteiras
    //  * Procura  cortes e executa heur�stica primal  (quando tiver sido
    //  *  solicitado) apenas  se o  LP  for �timo  e a  solu��o n�o  for
    //  * inteira. */
    // XPRSgetintattrib(prob,XPRS_LPSTATUS,&nLPStatus);
    // XPRSgetintattrib(prob,XPRS_MIPINFEAS,&nIntInf);
    // if (!(nLPStatus == 1 && nIntInf>0)) return 0;
    
    // /* Muda  o par�metro  SOLUTIONFILE para pegar  a solu��o do  LP da
    //     mem�ria. LEIA O MANUAL PARA ENTENDER este trecho */
    // XPRSgetintcontrol(prob,XPRS_SOLUTIONFILE,&solfile);
    // XPRSsetintcontrol(prob,XPRS_SOLUTIONFILE,0);
    // /* Pega a solu��o do LP. */
    // XPRSgetsol(prob,x,NULL,NULL,NULL);
    // /* Restaura de volta o valor do SOLUTIONFILE */
    // XPRSsetintcontrol(prob,XPRS_SOLUTIONFILE,solfile);

    // /* verifica o n�mero do n� em que se encontra */
    // XPRSgetintattrib(prob,XPRS_NODES,&node);

    // /* Imprime cabe�alho do n� */
    // printf("\n=========\n");
    // printf("N� %d\n",node);
    // printf("\n=========\n");
    // printf("La�o de separa��o: %d\n",itersep);
 
    // /* executa a heur�stica primal se for o caso */
    // if (HEURISTICA_PRIMAL) HeuristicaPrimal(node);

    // /* pega o valor otimo do LP ... */
    // XPRSgetdblattrib(prob, XPRS_LPOBJVAL,&lpobjval);

    // /* Imprime dados sobre o n� */
    // printf(".Valor �timo do LP: %12.6f\n",lpobjval);
    // printf(".Solu��o �tima do LP:\n");
    // ImprimeSol(x);
    // printf(".Rotina de separa��o\n");

    // /* guarda o valor da fun��o objetivo no primeiro n� */
    // if (node==1) objval_node1=lpobjval;

    //     /* guarda o valor da fun��o objetivo da primeira relaxa��o */
    // if ((node==1) && (!itersep)) objval_relax=lpobjval;

    // /* sai fora se for branch and bound puro */
    // if (!BRANCH_AND_CUT) return 0;

    // /*  sai  fora se  a  profundidade do  n�  corrente  for maior  que
    //  * MAX_NODE_DEPTH_FOR_SEP. */
    // xpress_ret=XPRSgetintattrib(prob,XPRS_NODEDEPTH,&node_depth);
    // if (xpress_ret) 
    //    errormsg("Cortes: erro na chamada da rotina XPRSgetintattrib.\n",__LINE__,xpress_ret);
    // if (node_depth > MAX_NODE_DEPTH_FOR_SEP) return 0;

    // /* vari�vel indicando se achou desigualdade violada ou n�o */
    // encontrou=0; 
    
    // /* carga dos parametros para a rotina de separacao da Cover Ineq */
    
    // /* ATENCAO:  A rotina Mochila nao  usa a posicao  zero dos vetores
    //    custo, peso  e sol,  portanto para carregar  o problema  e para
    //    pegar a solucao eh preciso acertar os indices */

    // peso=(int *)malloc(sizeof(int)*(n+1)); /* +1 !! */
    // assert(peso);
    // capacidade=0;

    // for(i=0;i<n;i++){
    //   peso[i+1]=w[i]; /* +1 !!! */
    //   capacidade=capacidade+peso[i+1]; /* +1 !! */
    // }
    // capacidade=capacidade-1-W;
    
    // /* calcula custos para o problema de separa��o (mochila) */
    // ajuste_val=0.0; /* ajuste para o custo do pbm da separa��o */
    // custo=(double *)malloc(sizeof(double)*(n+1));  /* +1 !!! */
    // assert(custo);
    // for(i=0;i<n;i++) {
    //   custo[i+1]=1.0-x[i]; 
    //   ajuste_val += custo[i+1];
    // }
    
    // /* aloca espaco para o vetor solucao da rotina Mochila */
    // sol=(int *)malloc(sizeof(int)*(n+1)); /* +1 !!! */
    // assert(sol);

    // /* resolve a mochila usando Programa��o Din�mica */
    // Mochila(custo,peso,capacidade,n,sol,&val);

    // /* calculo do RHS da desigualdade de cobertura */
    // irhs=0;
    // for(i=1;i<=n;i++)
    //   if (sol[i]==0) irhs++;
    
    // /* verifica se a desigualdade estah violada */
    // if (ajuste_val - val < 1.0-EPSILON) {
    //   encontrou=1;
      
    //   /* prepara a insercao do corte */
    //   mtype[0]=1;
    //   qrtype[0]='L';
    //   drhs[0]=(double)irhs - 1.0;
    //   mstart[0]=0; mstart[1]=irhs;
    //   k=0;
    //   for(i=1;i<=n;i++)
    // 	if (!sol[i]) {
    // 	  mcols[k]=i-1;   dmatval[k]=1.0;   k++;
    // 	}
    //   assert(k==irhs);

    //   /* Impress�o do corte */
    //   printf("..corte encontrado: (viol=%12.6f)\n\n   ",1.0-ajuste_val+val);
    //   for(i=0;i<irhs;i++){
    // 	printf("x[%d] ",mcols[i]);
    // 	if (i==irhs-1) printf("<= %d\n\n",irhs-1);
    // 	else printf("+ ");
    //   }

    //   /* adiciona o corte usando rotina XPRSaddcuts */
    //   xpress_ret=XPRSaddcuts(prob, 1, mtype, qrtype, drhs, mstart, mcols, dmatval);
    //   totcuts++;
    //   if (xpress_ret) 
    // 	errormsg("Cortes: erro na chamada da rotina XPRSgetintattrib.\n",__LINE__,xpress_ret);
      
    // }
    // else printf("..corte n�o encontrado\n");
    
    // assert(peso && sol && custo);
    // free(peso);      free(sol);         free(custo); 

    // printf("..Fim da rotina de cortes\n");

    // /* salva um arquivo MPS com o LP original */
    // xpress_ret=XPRSwriteprob(prob,"LPcuts","l");
    // if (xpress_ret) 
    //   errormsg("Cortes: rotina XPRSwriteprob.\n",__LINE__,xpress_ret);


    // if ((encontrou) && (itersep < MAX_ITER_SEP)) 
    //   { itersep++; ret=1; /* continua buscando cortes neste n� */ }
    // else
    //   { itersep=0; ret=0; /* vai parar de buscar cortes neste n� */}

    // return ret;
  return 0;
}

/**********************************************************************************\
*  Rotina que resolve uma mochila binaria por Programa��o Din�mica.
*  Autor: Cid Carvalho de Souza
*  Data: 10/2002
*
*  Objetivo: resolver uma mochila binaria maximizando o custo cx
*  e com uma restricao do tipo wx <= b.
*
*  Entrada: vetores $c$ (custos), $w$ (pesos), $b$ (capacidade)
*  e $n$ numero de itens.
*
*  Saidas: vetor $x$ (solucoes) e $z$ (valor otimo).
*
*  Observacao: todos os vetores sao inteiros, exceto os custos
*  que sao "double", assim como o valor otimo $z$.
*
*  IMPORTANTE: todos os  vetores comecam na posicao zero  mas os itens
*  s�o supostamente numerados de 1 a n. Assim, c[1] � o custo do item 1.
*
\**********************************************************************************/

void Mochila(double *c,int *w,int b,int n,int *x,double *val) {
  int k, d;

  double aux, limite=0.0;   /* variaveis auxiliares para calculos de custo */
  double **z;   /* matriz de resultados intermediarios da Prog. Din */

  /* aloca espaco para $z$ */
  z=(double **)malloc((n+1)*sizeof(double *));
  for(k=0;k<=n;k++) z[k]=(double *)malloc((b+1)*sizeof(double));


  /* inicializa o $z$ */
  for(k=0;k<=n;k++)
    for(d=0;d<=b;d++) z[k][d]=0.0;

  /* calculo do "limite" (numero negativo cujo valor absoluto eh 
     maior que o valor otimo com certeza): foi inicializado com ZERO. */
  for(k=1;k<=n;k++) limite=limite-c[k];

  /* prog. din.: completando a matriz z */
  for(k=1;k<=n;k++)
    for(d=1;d<=b;d++){
      /* aux sera igual ao valor de z[k-1][d-w[k]] quando esta celula 
         existir ou sera igual ao "limite" caso contrario. */
      aux = (d-w[k]) < 0 ? limite : z[k-1][d-w[k]] ;
      z[k][d] = (z[k-1][d] > aux+c[k]) ? z[k-1][d] : aux+c[k];
    }

  /* carrega o vetor solucao */
  for(k=0;k<=n;k++) x[k]=0;

  d=b;   k=n;
  while ((d!=0) && (k!=0)) {
    if (z[k-1][d] != z[k][d]) {
      d=d-w[k];   x[k]=1;
    }
    k--;
  }

  *val=z[n][b];

  /* desaloca espaco de $z$ */
  for(k=0;k<=n;k++) free(z[k]);
  free(z);

  return;
}

/**********************************************************************************\
*  Rotina  que encontra  uma solu��o  heur�stica para  mochila binaria
*  dada a solu��o e uma relaxa��o linear.
*
*  Autor: Cid Carvalho de Souza
*  Data: 10/2003
*
*  Entrada: a solu��o fracion�ria corrente � $x$ e o n� corrente sendo
*  explorado � o n� "node".
*
*  Id�ia da heur�stica: ordenar itens em ordem decrescente dos valores
*  de $x$. Em seguida ir construindo a solu��o heur�stica $xh$ fixando
*  as vari�veis em 1 ao varrer o vetor na ordem decrescente enquanto a
*  capacidade da mochila permitir.
*
\**********************************************************************************/

typedef struct{
  double valor;
  int indice;
} RegAux;

/* rotina auxiliar  de comparacao para o "qsort".  CUIDADO: Feito para
 * ordenar em ordem *DECRESCENTE* ! */
int ComparaRegAux(const void *a, const void *b) {
    int ret;
    if (((RegAux *)a)->valor > ((RegAux *)b)->valor) ret=-1;
    else {
	if (((RegAux *)a)->valor < ((RegAux *)b)->valor) ret=1;
	else ret=0;
    }
    return ret; 
}

void HeuristicaPrimal(int node){
  // /* vari�veis locais */
  // RegAux *xh;
  // double custo;
  // int i, j, Wresidual;

  // xh=(RegAux *)malloc(n*sizeof(RegAux));
  // for(i=0;i<n;i++){
  //   xh[i].valor=x[i];
  //   xh[i].indice=i;
  // }

  // qsort(xh,n,sizeof(RegAux),&ComparaRegAux);

  // /* calcula custo e  compara com a melhor solucao  corrente. Se for
  //  * melhor,  informa o  XPRESS sobre  o  novo incumbent  e salva  a
  //  * solucao. */
  // custo=0.0;
  // Wresidual=W;
  // for(i=0;(i<n) && (Wresidual > w[xh[i].indice]);i++){
  //   Wresidual -= w[xh[i].indice];
  //   custo += c[xh[i].indice];
  // }
  // /* Nota: ao final deste la�o "i-1"  ser� o maior �ndice (em xh !) de
  //    um item que cabe na mochila. */

  // /* Impress�o da solu��o heur�stica encontrada */
  // printf("..Solu��o Primal encontrada:\n");
  // for(j=0;j<i;j++)
  //   printf("item %3d, peso %6d, custo %12.6f\n",
  // 	   xh[j].indice,w[xh[j].indice],c[xh[j].indice]);
  // printf("               %6d        %12.6f\n",
  // 	 W-Wresidual,custo);

  // /* verifica se atualizar� o incumbent */
  // if (custo > zstar) {

  //   printf("..Heur�stica Primal melhorou a solu��o\n");

  //   for(j=0;j<i;j++) xstar[xh[i].indice]=1.0;
  //   for(j=i;j<n;j++) xstar[xh[i].indice]=0.0;
  //   zstar=custo;
    
  //   /* atualiza numero do n� onde encontrou a melhor solucao */
  //   NODE_BEST_INTEGER_SOL=node;
    
  //   /* informa xpress sobre novo incumbent */
  //   xpress_ret=XPRSsetdblcontrol(prob,XPRS_MIPABSCUTOFF,custo+1.0-EPSILON);
  //   if (xpress_ret) 
  //     errormsg("Heuristica Primal: Erro \n",__LINE__,xpress_ret);
  // }
  // else printf("..Heur�stica Primal n�o melhorou a solu��o\n");
  
  // free(xh);

  // return;
}

/*********************************************************************************\
*  Rotina  que imprime uma solu��o  heur�stica para  mochila binaria
*  dada a solu��o e uma relaxa��o linear.
*
*  Autor: Cid Carvalho de Souza
*  Data: 10/2003
\*********************************************************************************/
void ImprimeSol(double *a){
  // int i;
  // for(i=0;i<n;i++) if (a[i] > EPSILON)
  //   printf("x[%3d]=%12.6f (w[%3d]=%6d, c[%3d]=%12.6f)\n",i,a[i],i,w[i],i,c[i]);
}

/**********************************************************************************\
 * Rotina copiada de programas exemplo do XPRES ... :(
 * ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
 *
 * Name:         errormsg 
 * Purpose:      Display error information about failed subroutines.
 * Arguments:    const char *sSubName   Subroutine name             
 *               int nLineNo            Line number                 
 *               int nErrCode           Error code                  
 * Return Value: None                                               
\**********************************************************************************/
void errormsg(const char *sSubName,int nLineNo,int nErrCode)
{
   int nErrNo;             /* Optimizer error number */

   /* Print error message */
   printf("The subroutine %s has failed on line %d\n",sSubName,nLineNo);

   /* Append the error code, if it exists */
   if (nErrCode!=-1) printf("with error code %d\n",nErrCode);

   /* Append Optimizer error number, if available */
   if (nErrCode==32) {
      XPRSgetintattrib(prob,XPRS_ERRORCODE,&nErrNo);
      printf("The Optimizer error number is: %d\n",nErrNo);
   }

   /* Free memory, close files and exit */
   XPRSdestroyprob(prob);
   XPRSfree();
   exit(nErrCode);
}

