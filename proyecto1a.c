#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "queue.c"

/*
Proyecto Detector de Archivos Duplicados
Autor:
Xavier De Anta
*/

sem_t sem_visitar, sem_visitado, sem_readdir;
queue_t* visitar;
queue_t* visitado;
char mode;
// Registro que almacena el nombre del archivo visitado y su hash
typedef struct _file_reg{
	char* name;
	char hash[33];
	int count;
} R_file;

void* buscar_dirs(); //La funcion que van usar los hilos

int MDFile(char* filename, char hashValue[33]);
int check_visitar(queue_t*);
char* dircontent(char*, char*);
void compare_print(queue_t*);

int main(int argc, char** argv){
	char param;
	int nthreads,i;
	char init_path[256];
	char abs_path[1024];
	pthread_t* threads;
	getcwd(abs_path,sizeof(abs_path));
	//printf("Path Absoluto: %s\n", abs_path);
	//Leo los parametros de la entrada al programa
	while((param=getopt(argc, argv, "tdm:")) != -1){
		switch(param){
			case 't':
				nthreads=atoi(argv[optind]); //numero de hilos
			break;
			case 'd':
				strcpy(init_path,argv[optind]); //El directorio que se va a usar
			break;
			case 'm': //Modo de calculo del hash. Usando la libreria o el ejecutable para tal fin
				if(optarg[0]=='e' || optarg[0]=='l'){
					mode=optarg[0];
				}else{
					printf("Parametro invalido\n");
					exit(1);
				}
			break;
		}
	}
	threads=(pthread_t*)malloc(nthreads*sizeof(pthread_t)); //Creo el arreglo de hilos
	// Las 2 siguientes lineas crean las colas
	visitar=alloc_queue(); 
	visitado=alloc_queue();
	// Las 2 siguientes lineas crean los semaforos para visitar y visitado
	sem_init(&sem_visitar,0,1);
	sem_init(&sem_readdir,0,1);
	sem_init(&sem_visitado,0,1);

	queue_push_back(visitar,(void*)init_path); //Encolo el directorio de inicio
	//Printfs para debugging
	//printf("creando threads\n");
	//printf("nthreads:%d\n",nthreads);
	for(i=0;i<nthreads;i++){
		//Creo los hilos del programa
		//printf("Ciclo crear hilo\n");
		if(pthread_create(&threads[i], NULL, buscar_dirs, NULL) != 0){
			fprintf(stderr, "error al crear el hilo de ejecucion\n");
			exit(1);
		}
	}
	for(i=0;i<nthreads;i++){
		//Espero por los hilos hasta que terminen su ejecucion
		if(pthread_join(threads[i], NULL) != 0){
			perror("Error al esperar por el hilo");
			exit(1);
		}
	}
	compare_print(visitado);
	return 0;
}



void* buscar_dirs(){
	DIR* pdir; //Puntero de los directorios
	struct dirent* spdirs; //Contenido del directorio
	struct stat buffer; //struct para usar la funcion lstat(), funcion que me dice que tipo de archivo es
	char* entity;
	char* temp;
	int status,status_md5;
	entity=(char*)calloc(256,sizeof(char));
	while(check_visitar(visitar) > 0){
		//Accesos a la cola es una Seccion critica
		sem_wait(&sem_visitar);
		strcpy(entity,queue_pop_front(visitar));
		sem_post(&sem_visitar);
		status=lstat(entity,&buffer);
		//printf("status: %d; entity: %s\n",status, entity);
		if(status==0){
			if(S_ISDIR(buffer.st_mode)){
				/*Si es un directorio, lo abro leo todo el contenido del mismo y los encolo*/
				pdir=opendir(entity);
				while((spdirs=readdir(pdir))!=NULL){
					if(strcmp(spdirs->d_name,".") != 0 && strcmp(spdirs->d_name,"..") != 0){
						//Ignoro los directorios "." y ".."
						//Dircontent es una funcion que concatena el nombre del directorio con los archivos del mismo
						queue_push_back(visitar,(void*)dircontent(entity,spdirs->d_name));
					}
				}
				sem_post(&sem_readdir);
			}else{
				if(S_ISREG(buffer.st_mode)){
					/* Si es un archivo, guardo su nombre, calculo su hash y lo guardo en otra cola*/
					R_file* f;
					f=(R_file*)malloc(sizeof(R_file));
					f->name=(char*)calloc(256,sizeof(char));
					f->count=0;
					strcpy(f->name, entity);
					if(buffer.st_size > 0){
						sem_wait(&sem_visitado);
						// Si es modo libreria solo llamo a la funcion MDFile para calcular su hash
						if(mode == 'l'){
							status_md5=MDFile(entity,f->hash);
						}else{
							//Si es modo ejecutable Creo el pipe primero antes del fork();

							int fd[2];
							FILE* fpipe;
							pipe(fd);
							pid_t child;
							if((child=fork())==0){
								//Codigo del hijo
								//Cierro la lectura del pipe
								close(fd[0]);
								//dup2 cierra la salida estandar y redirije la salida del programa al pipe creado
								dup2(fd[1],1);
								//Ejecuto el programa para calcular su hash
								execlp("./md5","md5",entity, NULL);
							}else{
								//Codigo del proceso padre
								//Cierro la escritura sobre el pipe
								close(fd[1]);
								//Espero a que termine la ejecucion del md5
								wait(NULL);
								//Abro el pipe para leer su contenido
								fpipe=fdopen(fd[0],"r");
								//Extraigo la informacion
								fscanf(fpipe,"%s",f->hash);
								//Cierro el pipe
								fclose(fpipe);
							}
							//printf("Archivo:%s; hash:%s\n", entity, f->hash);
						}
						//printf("Archivo:%s; hash:%s\n", entity, f->hash);
						//Encolo el archivo con su hash
						if(queue_size(visitado) == 0){
							queue_push_back(visitado,(void*)f);
							//printf("Cola vacia\n");
						}else{
							int i;
							R_file* temp;
							//printf("else\n");
							for(i = 0; i < queue_size(visitado); i++){
								temp=(R_file*)queue_at(visitado,i);
								if(strcmp(f->hash, temp->hash) == 0){
									temp->count++; //Si es un archivo repetido, Aumento el contador del archivo ya registrado, el contador de la copia es 0
									//printf("Count de %s: %d\n", f->name, f->count);
									//f->count=temp->count;
									//queue_push_back(visitado,(void*)f);
								}
							}
							queue_push_back(visitado,(void*)f); //Encolo los archivos encontrados en el directorio
						}
						//queue_push_back(visitado,(void*)f);
						//free(f);
						sem_post(&sem_visitado);
					}
				}
			}
		}
		//sem_post(&sem_visitar);
	}
	pthread_exit(NULL);
}

int check_visitar(queue_t* a){
	int ret;
	sem_wait(&sem_visitar);
	ret=queue_size(a);
	sem_post(&sem_visitar);
	return ret;
}

char* dircontent(char* source, char* content){
	char* string;
	string=(char*)calloc(256,sizeof(char));
	strcpy(string,source);
	strcat(string,"/");
	strcat(string, content);
	return string;
}

void compare_print(queue_t* visit){ //Compare_print revisa la cola, cuenta el total de archivos repetido y muestra por pantalla los nombres de los archivos
	queue_t* filter;
	int total_copy,ind,j;
	R_file* aux;
	R_file* aux2;
	filter=alloc_queue();
	//f=queue_pop_front(visit);
	total_copy=0;
	ind=0;
	for(ind=0; ind < queue_size(visit); ind++){ //El total sale de los contadores de los archivos registrados en la cola, los contadores de las copias es 0
		aux=(R_file*)queue_at(visit,ind);
		total_copy=total_copy+aux->count;
	}
	printf("Se han encontrado %d archivos duplicados.\n",total_copy);
	while(queue_size(visit) > 0){
		aux=(R_file*)queue_pop_front(visit);
		for(j=0;j<queue_size(visit);j++){
			if(aux->count > 0){ //Ignora los archivos copia que esten en la cola
				aux2=(R_file*)queue_at(visit,j);
				if(strcmp(aux->hash,aux2->hash) == 0){
					printf("%s es duplicado de %s\n",aux2->name, aux->name );
				}
			}
		}
	}
}