/*****************************************************
 * Copyright Grégory Mounié 2008-2015                *
 *           Simon Nieuviarts 2002-2009              *
 * This code is distributed under the GLPv3 licence. *
 * Ce code est distribué sous la licence GPLv3+.     *
 *****************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <wordexp.h>

#include "variante.h"
#include "readcmd.h"

#ifndef VARIANTE
#error "Variante non défini !!"
#endif

/* Guile (1.8 and 2.0) is auto-detected by cmake */
/* To disable Scheme interpreter (Guile support), comment the
 * following lines.  You may also have to comment related pkg-config
 * lines in CMakeLists.txt.
 */

#if USE_GUILE == 1
#include <libguile.h>

/* Structure définissant une liste de processus (pid et commande) */
struct _proc {
    int pid;
		char **cmd;
    struct _proc * next;
};
typedef struct _proc * proc;

/* Liste chaînée des processus s'exécutant en tâche de fond */
proc tab_proc;

/* Ajoute le processus pid exécutant la commande cmd à la liste de
	processus tab */
void ajouter(int pid, char** cmd, proc *tab){
	proc new = malloc(sizeof(new));
	new->pid = pid;
  new->cmd = malloc(10*sizeof(char *));
  int i;
  for(i = 0; cmd[i] != NULL; i++){
    new->cmd[i] = malloc(strlen(cmd[i])*sizeof(char));
    strcpy(new->cmd[i],cmd[i]);
  }
  new->cmd[i] = NULL;
	new->next = *tab;
	*tab = new;
}

/* Enlève le processus pid de la liste de processus tab et le libère */
proc enlever(int pid, proc tab){
	if(tab != NULL){
		proc p = tab;

		if(p->pid == pid){
			if(p->next != NULL) tab = tab->next;
			else tab = NULL;
      int i;
      for(i = 0; p->cmd[i] != 0; i++) free(p->cmd[i]);
      free(p->cmd[i]);
      free(p->cmd);
			free(p);
			return tab;
		}
		while(p->next != NULL){
			proc q = p;
			p = p->next;
			if(p->pid == pid){
				if(p->next != NULL) q->next = p->next;
				else q->next = NULL;
        int i;
        for(i = 0; p->cmd[i] != 0; i++) free(p->cmd[i]);
        free(p->cmd[i]);
        free(p->cmd);
  			free(p);
				return tab;
			}
		}
		return tab;
	}
	return tab;
}

/* Affiche les processus de la liste de processus tab et leur commande */
void affiche(proc tab){
	if(tab != NULL){
		proc p = tab;
		while(p->next != NULL){
			printf("pid : %d\n",p->pid);
      printf("commande :");
      for(int j = 0; p->cmd[j] != 0; j++) {
        printf(" %s",p->cmd[j]);
      }
      printf("\n");
			p = p->next;
		}
		printf("pid : %d\n",p->pid);
    printf("commande :");
    for(int j = 0; p->cmd[j] != 0; j++) {
      printf(" %s",p->cmd[j]);
    }
    printf("\n");
	}
}

/* Renvoie un tableau de mots explicitant les jokers et les variables
   d'environnement pour l'invite de commande. c représente la commande avec
   jokers et variables d'environnement sous forme implicite */
char **joker(char **c){
  if(c){
    char **res = malloc(10*sizeof(char *));
    res[0] = c[0];
    wordexp_t p;
    int indice = 1;
    for(int i = 1; c[i] != NULL; i++){
      wordexp(c[i],&p,0);
      for(int j = 0; j < p.we_wordc; j++){
        res[indice] = malloc(strlen(p.we_wordv[j])*sizeof(char));
        strcpy(res[indice],p.we_wordv[j]);
        indice++;
      }
    }
    res[indice] = NULL;
    if(indice > 1) wordfree(&p);
    return res;
  }
  return c;
}

/* Crée un pipe puis fork. Permet de créer autant de pipe que nécessaire par
   récursivité. L'indice dad indique au père quelle séquence il doit exécuter.
   A la 1ere occurence, dad est le dernière indice pointant sur une sequence
   non-nulle du tableau l->seq */
void connect_tube(struct cmdline *l, int dad){

  int tube[2];
  pipe(tube);

  pid_t pid;

  switch(pid = fork()){
    case 0:
      dup2(tube[1],1);
      close(tube[0]);
      close(tube[1]);
      if(dad > 1) connect_tube(l,dad-1);
      execvp(l->seq[0][0],joker(l->seq[0]));
      printf("[ERREUR] commande inconnue\n");
      exit(-1);
      break;
    case -1:
      perror("[ERREUR] fork\n");
      exit(-1);
      break;
    default:
      dup2(tube[0],0);
      close(tube[1]);
      close(tube[0]);
      execvp(l->seq[dad][0],joker(l->seq[dad]));
      printf("[ERREUR] commande inconnue\n");
      exit(-1);
      break;
  }
}

/* Renvoie le nombre de séquences de l, i.e le premier indice pointant sur une
   case nulle du tableau l->seq */
int nb_seq(struct cmdline *l){
  int i = 0;
  while(l->seq[i]) i++;
  return i;
}

/* Fonction bloc appelant fork(), execvp(l->seq[0][0], joker(l->seq[0])) ou
   connect_tube(l,nb_seq(l)-1) et ouvrant et fermant les fichiers l->in et
   l->out si besoin. Gère également les processus en tâche de fond et jobs */
void cmd_fork(struct cmdline *l){
  int status;
  int f_entree = -1;
  int f_sortie = -1;
  pid_t pid;
  switch(pid = fork()){
    case 0:
      if (l->in){
        if((f_entree = open (l->in, O_RDONLY)) != -1){
          close(0);
          dup2(f_entree, 0);
        }
        else{
          printf("ERREUR: Ouverture fichier %s\n",l->in);
          exit(-1);
        }
      }
      if (l->out){
        if((f_sortie = open (l->out, O_WRONLY | O_CREAT | O_TRUNC, S_IRWXU)) != -1){
          close(1);
          dup2(f_sortie, 1);
        }
        else{
          printf("ERREUR: Ouverture fichier %s\n",l->out);
          exit(-1);
        }
      }
      if(l->seq[1]) connect_tube(l,nb_seq(l)-1);
      else{
        execvp(l->seq[0][0], joker(l->seq[0]));
        printf("[ERREUR] commande inconnue\n");
        exit(-1);
        break;
      }
    case -1:
      perror("[ERREUR] fork\n");
      exit(-1);
      break;
    default:
      if(l->bg){
        ajouter(pid,l->seq[0],&tab_proc);
      }
      else wait(&status);
      break;
  }

  if(f_entree > 0) close(f_entree);
  if(f_sortie > 0) close(f_sortie);
}

void terminate(char *line) {
#if USE_GNU_READLINE == 1
	/* rl_clear_history() does not exist yet in centOS 6 */
	clear_history();
#endif
	if (line)
	  free(line);
	printf("exit\n");
	exit(0);
}

int question6_executer(char *line){
  struct cmdline *l;

  l = parsecmd( & line);

  if (!l) terminate(0);

  if (l->err) {
    printf("error: %s\n", l->err);
    exit(-1);
  }

  if(!strcmp(l->seq[0][0],"jobs")) affiche(tab_proc);
  else cmd_fork(l);

  return 0;
}

SCM executer_wrapper(SCM x)
{
        return scm_from_int(question6_executer(scm_to_locale_stringn(x, 0)));
}
#endif

/* Vide la liste tab_proc des processeurs ayant fini leur exécution */
void check_background(){
  if(tab_proc != NULL){
    int status;
    int etat;
    proc p = tab_proc;
    while(p->next != NULL){
      switch(etat = waitpid(p->pid,&status,WNOHANG)){
        case -1:
          perror("[ERREUR] waitpid(WNOHANG)");
          break;
        case 0:   //waitpid renvoie 0 si le processus n'a pas fini
          break;
        default:
          tab_proc = enlever(etat,tab_proc);
          break;
      }
      p = p->next;
    }
    switch(etat = waitpid(p->pid,&status,WNOHANG)){
      case -1:
        perror("[ERREUR] waitpid(WNOHANG)");
        break;
      case 0:
        break;
      default:
        tab_proc = enlever(etat,tab_proc);
        break;
    }
  }
}

int main() {
        printf("Variante %d: %s\n", VARIANTE, VARIANTE_STRING);

#if USE_GUILE == 1
        scm_init_guile();
        /* register "executer" function in scheme */
        scm_c_define_gsubr("executer", 1, 0, 0, executer_wrapper);
#endif

	while (1) {
		struct cmdline *l;
		char *line=0;
		char *prompt = "ensishell>";

		/* Readline use some internal memory structure that
		   can not be cleaned at the end of the program. Thus
		   one memory leak per command seems unavoidable yet */
		line = readline(prompt);

    check_background();

		if (line == 0 || ! strncmp(line,"exit", 4)) {
			terminate(line);
		}

#if USE_GNU_READLINE == 1
		add_history(line);
#endif


#if USE_GUILE == 1
		/* The line is a scheme command */
		if (line[0] == '(') {
			char catchligne[strlen(line) + 256];
			sprintf(catchligne, "(catch #t (lambda () %s) (lambda (key . parameters) (display \"mauvaise expression/bug en scheme\n\")))", line);
			scm_eval_string(scm_from_locale_string(catchligne));
			free(line);
                        continue;
                }
#endif

		/* parsecmd free line and set it up to 0 */
		l = parsecmd( & line);

		/* If input stream closed, normal termination */
		if (!l) terminate(0);

		if (l->err) {
			/* Syntax error, read another command */
			printf("error: %s\n", l->err);
			continue;
		}

		if(!strcmp(l->seq[0][0],"jobs")) affiche(tab_proc);
		else cmd_fork(l);
	}

}
