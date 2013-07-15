#include<stdio.h>
#include<math.h>
#include<stdlib.h>
#include<commons/config.h>
#include<string.h>
#include<configuracion.h>
#include<header_clie_serv.h>
#include <signal.h>
#include<commons/collections/dictionary.h>

t_config_personaje* leerConfiguracion(char*,int*);

/*Funciones Con socket*/
void ingresarAlNivel(int*,int*,char*,char*,char);
void esperar_notificacion(int);
void obtener_destino(int,int*,int*,char*);
void informar_nueva_posicion(int,int,int,char);
int solicitar_instancia(int,char*);
void realizar_notificacion(int,int);
void desconectar_nivel(int,int);
void recorrerRecursosProceso(int,int,t_config_personaje* pers,int*);
void recorrerNivelesProceso(t_config_personaje* pers);

void moverPersonaje(int*,int*,int,int);//Recibe posicion XY personaje y objetivo, actualiza posicion personaje
int calcularSentido(int);	//Recibe diferencia de posicion respecto un eje y devuelve el sentido a moverse. ej: Recibe -15 Devuelve 1; Rec 3 Dev 1
void efectuarMovimiento(int*,int);
int llegoDestino(int,int,int,int);
void tratarMuerte(int);
void incrementarVida(int);
void morir (int *,int *,int *);
/* ************************************************** */
int puerto(char*);
char *ip(char*);
/* ************************************************** */


int vidaActual;
int vidaInicial;
int salir=0;

int main(int argc,char *argv[]){
	int ok;
	if (argc!=2){
		printf("Debe tener un argumento con el nombre del archivo de Configuracion\n");
	}
	else{
		t_config_personaje* pers;
		pers = leerConfiguracion(argv[1],&ok);
		if(ok){
			//Inicializa  Vida
			vidaActual = vidaInicial = pers->vidas;
			recorrerNivelesProceso(pers);
		}
	}
	return 0;
}

void recorrerNivelesProceso(t_config_personaje* pers){
	int proximoNivel=0;
	int socketNivel;
	int socketPlanificador;
    signal(SIGTERM, tratarMuerte);
    signal(SIGUSR1,incrementarVida);
	while(((pers->objNivel+proximoNivel)->numeroNivel)!=NULL){ //Recorre los Niveles
		ingresarAlNivel(&socketNivel,&socketPlanificador,pers->orquestador, ((pers->objNivel+proximoNivel)->numeroNivel),pers->simbolo);//INGRESA AL NIVEL (IPORQUESTADOR NIVEL)
		recorrerRecursosProceso(socketNivel,socketPlanificador,pers,&proximoNivel);//Recorre todos los recursos del nivel indicado/
		desconectar_nivel(socketNivel,socketPlanificador);//Desconecta del Nivel y planificador de Nivel por alcanzar total de recursos o muerte
		proximoNivel++; //Avanza al proximo nivel del Plan
	}
	/* Sale del ciclo porque completo plan de Nivel */
}
void recorrerRecursosProceso(int socketNivel,int socketPlanificador,t_config_personaje* pers,int *proximoNivel){
	int posicionCajaX,posicionCajaY;
	int posicionX=1; 		//inicializa posicion personaje
	int posicionY=1;
	int recurso=0;
	int destino=0;
	int otorgada;
	int tipoNotificacion;
	char **objetivos=((pers->objNivel+(*proximoNivel))->objetivos);//Esto es para recorrerlo mas facil

	while(*(objetivos+recurso)!=NULL){ //Recorre los objetivos
		//Realiza Movimiento
		//printf("MI POSICION %d %d\n",posicionX,posicionY);
		if (!destino) //Pregunta si tiene destino
			{obtener_destino(socketNivel,&posicionCajaX,&posicionCajaY,*(objetivos+recurso));  // solicita destino (posicionX,Y recurso)
			destino=1;//Bandera que SI tiene destino
			printf("OBTIENE DESTINO \n");
			}
		printf("POR RECIBIR NOTIF \n");
		esperar_notificacion(socketPlanificador);//Recibir Movimiento Permitido del Planificadir (Se bloquea hasta que recv la indicacion)
		printf("RECIBI NOTIF\n");
		// Efectua Movimiento
		moverPersonaje(&posicionX,&posicionY,posicionCajaX,posicionCajaY);
		printf("MI POSICION %d %d\n",posicionX,posicionY);
		informar_nueva_posicion(socketNivel,posicionX,posicionY,pers->simbolo);//  Informa a nivel nueva posicion para graficarla
		tipoNotificacion=1;
		if (llegoDestino(posicionX,posicionY,posicionCajaX,posicionCajaY))
			{ otorgada =solicitar_instancia(socketNivel,*(objetivos+recurso));//Solicita Instancia y recibir respuesta
				 if (otorgada)
				 {	printf("RECIBI \n");
					 tipoNotificacion=2;

				 }
			     else
				 {
					 printf("ME BLOQUEE \n");
					 tipoNotificacion=3;
				 }
		  		 destino=0; //No hay proximo Destino
		  		 recurso++;	//Apuntar proximo Recurso dentro de nivel
			}
		realizar_notificacion(socketPlanificador,tipoNotificacion);//Notifica al Planificador "concluyo turno" tipoNotificacion 1 Normal  2 Solicite y me lo dio 3 Bloqueado
		if(salir){
			morir(&destino,&recurso,proximoNivel);
			salir=0;
			break;
		}
	}
	/*Sale por  No tener proximo objetivo, es decir alcanzo total de Recursos O en CASO DE MUERTE*/
}



int calcularSentido(int x){
return (x/abs(x));
}
void efectuarMovimiento(int *posicion,int movimiento){
(*posicion)+= movimiento;
}

void moverPersonaje(int *posX,int *posY,int posCajaX,int posCajaY){
	int movimiento = posCajaX - *posX;
	if (movimiento)
		{	/*Realiza movimiento X*/
			efectuarMovimiento(posX,calcularSentido(movimiento));}
		else {	movimiento=posCajaY - *posY;
				if (movimiento)
				{	/*Realiza movimiento Y*/
					efectuarMovimiento(posY,calcularSentido(movimiento));}
			 }
}

t_config_personaje *leerConfiguracion(char* nombreArchivo,int* ok){
	t_config_personaje *pers=(t_config_personaje*)malloc(sizeof( t_config_personaje));

	t_config *personaje;

	 personaje = asociarArchivoFisico(nombreArchivo);
	 if (dictionary_size(personaje->properties)){
		 pers->nombre = obtenerNombre(personaje);
		 pers->simbolo = obtenerSimbolo(personaje);
		 pers->planDeNiveles = planDeNiveles(personaje);
		 pers->objNivel = obtenerObjetivos(personaje,pers->planDeNiveles);
		 pers->vidas = obtenerVidas(personaje);
		 pers->orquestador = obtenerOrquestador(personaje);
		 *ok=1;
		 /*destruirEstructura(personaje);*/
	 }
	 else{
		printf("No se a encontrado: %s\n",nombreArchivo);
		*ok=0;
	 }
return pers;
}


int llegoDestino(int X,int Y,int CX,int CY){
	return ((X==CX) && (Y==CY));
}

void ingresarAlNivel(int *socketNivel,int *socketPlanificador,char* dirOrquestador,char *nivel,char simbolo){
	int socketOrquestador;

	int nbytes;
	char buffer[1024];

	socketOrquestador =conectar_a_servidor("127.0.0.1",9099);

	sprintf(buffer,"DAME IP PUERTO NIVEL Y PALNIF %s",nivel);
		if (send(socketOrquestador, buffer, strlen(buffer), 0) >= 0) {
			} else {
			perror("Error al enviar datos. Server no encontrado.\n");
			}

		if ((nbytes = recv(socketOrquestador, buffer, sizeof(buffer), 0)) <= 0) {
 						if (nbytes == 0) {
 							printf("cliente: socket %d terminó conexion\n", socketOrquestador);
 						} else {
 							perror("recv");
 						}
  					} else {
  						//Recibir ip/puesto planif y nivel (y almacenar en variable
printf("%s","ACA TENES IP DE NIVEL Y PLANIF \n");
					}

	close(socketOrquestador);	//cerrar Conexion orquestador

	//conectar a planificador y nivel

	int puertoNivel=5054;
	char ipNivel[16]="127.0.0.1";
	int puertoPlanificador=9034;
	char ipPlanificador[16]="127.0.0.1";

	*socketNivel=conectar_a_servidor(ipNivel,puertoNivel);

	sprintf(buffer,"%s%c0%d0%d","1",simbolo,1,1);

//le aviso a nivel posicion inicial y simbolo
	if (send(*socketNivel, buffer, strlen(buffer), 0) >= 0) {
		} else {
		perror("Error al enviar datos. Server no encontrado.\n");
		}
	*socketPlanificador=conectar_a_servidor(ipPlanificador,puertoPlanificador);
	//HANDSHAKE... HOLA! SOY ARROBA , QUE TAL?
	sprintf(buffer,"%c",simbolo);
	if (send(*socketPlanificador, buffer, strlen(buffer), 0) >= 0) {
		} else {
		perror("Error al enviar datos. Server no encontrado.\n");
		}

}

void esperar_notificacion(int socketPlanificador){
	int nbytes;
	char buffer[1024];
	/****************************************************************/
//strcpy(buffer,"PLANIFICADOR: ESPERO NOTIFICACIONN \n");send(socketPlanificador, buffer, strlen(buffer), 0);
	/****************************************************************/
	//Recibe instruccion de permitido del Planificador
	if ((nbytes = recv(socketPlanificador, buffer, sizeof(buffer), 0)) <= 0) {
						if (nbytes == 0) {
							printf("cliente: socket %d terminó conexion\n", socketPlanificador);
						} else {
							perror("recv");
						}
					} else {
						//EL BUFFER DICE MOVETE?
						fwrite(buffer, 1, nbytes, stdout);
						//printf("%s\n",buffer);
				}
}
void realizar_notificacion(int socketPlanificador,int tipoNotificacion){
	char buffer[1024];
	int nbytes;
	switch(tipoNotificacion){
		case 1:{
			strcpy(buffer,"TERMINE");
			break;
		}
		case 2:{
			strcpy(buffer,"SOLICITE");
			break;
		}
		case 3:{
			strcpy(buffer,"BLOQUEADOF");
			break;
		}
	}

	if (send(socketPlanificador, buffer, strlen(buffer), 0) >= 0) {
		} else {
		perror("Error al enviar datos. Server no encontrado.\n");
		}
	//Envia al planificador que concluyo Turno

	/*****************************************************************/
	//nbytes = recv(socketPlanificador, buffer, sizeof(buffer), 0);
			/*******************************************/

}
void obtener_destino(int socketNivel,int *x,int *y,char *recurso){
		int nbytes;
		char buffer[1024];
		char px[3],py[3];

	sprintf(buffer,"3%s",recurso);
	//Solicita a Nivel Posicion del recurso
	printf("NIVEL VOY A PEDIR DESTINO \n");
	if (send(socketNivel, buffer, strlen(buffer), 0) >= 0) {
		} else {
		perror("Error al enviar datos. Server no encontrado.\n");
		}
	//Recibe la Posicion del recurso de Nivel
	if ((nbytes = recv(socketNivel, buffer, sizeof(buffer), 0)) <= 0) {
						if (nbytes == 0) {
							printf("cliente: socket %d terminó conexion\n", socketNivel);
						} else {
							perror("recv");
						}
					} else {//fwrite(buffer, 1, nbytes, stdout);
						//printf("%s",buffer);
								px[0]=buffer[0];
								px[1]=buffer[1];
								px[2]='\0';
								py[0]=buffer[2];
								py[1]=buffer[3];
								py[2]='\0';
						//Recibe la Posicion del recurso de Nivel
				}
	(*x)=atoi(px);(*y)=atoi(py);
	printf("ME DIO DESTINO %d %d\n",*x,*y);
}
void informar_nueva_posicion(int socketNivel,int x,int y,char simbolo){
	char buffer[1024];
	char px[3],py[3];
	//Envia nuava posicion
/********************esta parte es fantasma.. provisorio hasta serializacion */
	if(x>9)
		sprintf(px,"%d",x);
	else sprintf(px,"0%d",x);
	if(y>9)
		sprintf(py,"%d",y);
	else sprintf(py,"0%d",y);

sprintf(buffer,"%s%s%s","2",px,py);

if (send(socketNivel, buffer, strlen(buffer), 0) >= 0) {
	} else {
	perror("Error al enviar datos. Server no encontrado.\n");
	}

}


int solicitar_instancia(int socketNivel,char* recurso){
	int nbytes;
	int respuesta;
	char buffer[1024];
	char aux[2];
	//Solicita Instancia
	sleep(1);

sprintf(buffer,"4%s",recurso);
if (send(socketNivel, buffer, strlen(buffer), 0) >= 0) {
	} else {
	perror("Error al enviar datos. Server no encontrado.\n");
	}

//recibe respuesta
if ((nbytes = recv(socketNivel, buffer, sizeof(buffer), 0)) <= 0) {
					if (nbytes == 0) {
						printf("cliente: socket %d terminó conexion\n", socketNivel);
					} else {
						perror("recv");
					}
				} else {aux[0]=buffer[0];
						aux[1]='\0';
						respuesta=atoi(aux);
					//Recibe respuesta ok o bloqueado
			}
return respuesta;
}
void desconectar_nivel(int socketNivel,int socketPlanificador){
	close(socketNivel);
	close(socketPlanificador);
	sleep(1);
	//desconecta de nivel y planificador
}

void morir (int *destino,int *recurso,int *proximoNivel){
	*destino=0;
	*recurso=0;
	if (vidaActual>0){
		vidaActual-=1;
		(*proximoNivel)-=1;
	}
	else {
		*proximoNivel=-1;
		vidaActual=vidaInicial;
	}
}
void incrementarVida(int senial){
	vidaActual+=1;
}
void tratarMuerte(int senial){
	salir=1;
}

/* *************************************************** */
int puerto(char* cadena){
	int i,puerto;
	for(i=0;*(cadena+i)!=':';i++);
		puerto=atoi(cadena+(i+1));
	return puerto;
}

char* ip(char* cadena){
	int i;
	char *ip=(char*)malloc(sizeof(char)*15);
	for(i=0;*(cadena+i)!=':';i++)
		*(ip+i)=*(cadena+i);
	return ip;
}

