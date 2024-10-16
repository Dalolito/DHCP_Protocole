#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <time.h>
#include <signal.h>  // Para manejar señales como Ctrl + C

#define SERVER_PORT 1067         // Puerto donde escucha el servidor DHCP
#define CLIENT_PORT 68           // Puerto donde escucha el cliente DHCP
#define SERVER_IP "127.0.0.1"    // IP del servidor (usando localhost para pruebas)
#define MAX_RENEWALS 4           // Máximo número de renovaciones

// Estructura para un mensaje DHCP (simplificada para el ejemplo)
typedef struct {
    int message_type;         // Tipo de mensaje (1 = DHCPDISCOVER, 3 = DHCPREQUEST, 5 = DHCPRELEASE)
    char client_mac[18];      // Dirección MAC del cliente (formato: XX:XX:XX:XX:XX:XX)
    char requested_ip[16];    // Dirección IP solicitada por el cliente
    uint8_t options[312];     // Campo para recibir las opciones DHCP
} dhcp_message;

uint32_t lease_time = 0;       // Variable para almacenar el lease time recibido
time_t lease_start_time;       // Marca de tiempo cuando se recibe la IP

int sockfd;                    // Descriptor del socket global para que se pueda usar en la señal
char assigned_ip[16];          // Para almacenar la IP asignada globalmente
struct sockaddr_in server_addr; // Estructura para la dirección del servidor
socklen_t addr_len = sizeof(server_addr); // Tamaño de la estructura del servidor

// Función para enviar DHCPRELEASE al servidor
void send_dhcp_release() {
    dhcp_message release_msg;
    memset(&release_msg, 0, sizeof(release_msg));
    release_msg.message_type = 5; // 5 = DHCPRELEASE
    strcpy(release_msg.client_mac, "00:11:22:33:44:55"); // Simulación de la MAC
    strcpy(release_msg.requested_ip, assigned_ip);       // IP asignada previamente

    printf("Enviando DHCPRELEASE para la IP %s...\n", assigned_ip);
    if (sendto(sockfd, &release_msg, sizeof(release_msg), 0, (struct sockaddr*)&server_addr, addr_len) < 0) {
        perror("Error al enviar DHCPRELEASE");
    } else {
        printf("DHCPRELEASE enviado para la IP %s\n", assigned_ip);
    }
}

// Manejador de señal para liberar la IP cuando el cliente sea interrumpido (Ctrl + C)
void signal_handler(int signum) {
    send_dhcp_release();  // Enviar DHCPRELEASE antes de cerrar el cliente
    close(sockfd);        // Cerrar el socket
    exit(0);              // Salir del programa
}

// Función para imprimir las opciones recibidas del servidor
void print_dhcp_options(dhcp_message *response) {
    uint8_t *options = response->options;
    int offset = 0;

    while (options[offset] != 255) { // 255 es el fin de opciones
        int option_code = options[offset++];
        int option_length = options[offset++];

        switch (option_code) {
            case 53: // Tipo de mensaje DHCP
                printf("Tipo de mensaje DHCP: ");
                switch (options[offset]) {
                    case 1:
                        printf("DHCPDISCOVER\n");
                        break;
                    case 2:
                        printf("DHCPOFFER\n");
                        break;
                    case 3:
                        printf("DHCPREQUEST\n");
                        break;
                    case 4:
                        printf("DHCPACK\n");
                        break;
                    case 5:
                        printf("DHCPNAK\n");
                        break;
                    case 6:
                        printf("DHCPRELEASE\n");
                        break;
                    case 7:
                        printf("DHCPINFORM\n");
                        break;
                    default:
                        printf("Desconocido (%d)\n", options[offset]);
                        break;
                }
                break;

            case 51: // Tiempo de concesión (lease time)
                printf("Tiempo de concesión: ");
                memcpy(&lease_time, &options[offset], sizeof(lease_time));
                lease_time = ntohl(lease_time); // Convertir de formato de red a formato local
                printf("%u segundos\n", lease_time);
                lease_start_time = time(NULL); // Guardar el momento en que se asigna la IP
                break;

            // Otras opciones como la máscara de subred, puerta de enlace, DNS, etc.
        }

        offset += option_length; // Mover el offset al siguiente par código-longitud
    }
}

// Función para calcular el tiempo de renovación (T1)
int time_to_renew() {
    return (lease_time / 2); // T1: 50% del tiempo de concesión
}

// Función para renovar la IP enviando DHCPREQUEST
void renew_ip(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len, const char *client_mac, const char *requested_ip) {
    dhcp_message msg;
    memset(&msg, 0, sizeof(msg));
    msg.message_type = 3; // 3 = DHCPREQUEST
    strcpy(msg.client_mac, client_mac);
    strcpy(msg.requested_ip, requested_ip);

    printf("Renovando IP %s...\n", requested_ip);
    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)server_addr, addr_len) < 0) {
        perror("Error al enviar DHCPREQUEST para renovar");
    } else {
        printf("DHCPREQUEST enviado para renovar IP %s\n", requested_ip);
    }
}

// Función para manejar la respuesta del servidor durante la renovación
int handle_renewal_response(int sockfd, struct sockaddr_in *server_addr, socklen_t addr_len) {
    dhcp_message response;

    if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr*)server_addr, &addr_len) < 0) {
        perror("Error al recibir respuesta del servidor durante la renovación");
        return -1;  // Error en la renovación
    }

    if (response.message_type == 4) { // DHCPACK
        printf("Renovación de IP aceptada (DHCPACK recibido)\n");
        return 1;  // Renovación exitosa
    } else if (response.message_type == 5) { // DHCPNAK
        printf("Renovación de IP rechazada (DHCPNAK recibido)\n");
        return 0;  // Renovación rechazada
    }

    return -1;  // Respuesta inesperada
}

int main() {
    int renewals = 0;  // Contador de renovaciones
    struct sockaddr_in client_addr;
    dhcp_message msg, response;

    // Crear el socket UDP
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("No se pudo crear el socket");
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del cliente
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(0); // Permitir al sistema asignar un puerto dinámico

    // Enlazar el socket del cliente
    if (bind(sockfd, (struct sockaddr*)&client_addr, sizeof(client_addr)) < 0) {
        perror("No se pudo enlazar el socket del cliente");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Configurar la dirección del servidor
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    // Manejar la señal para enviar DHCPRELEASE al terminar
    signal(SIGINT, signal_handler);

    // Enviar mensaje DHCPDISCOVER al servidor
    msg.message_type = 1; // 1 = DHCPDISCOVER
    strcpy(msg.client_mac, "00:11:22:33:44:55"); // Simulación de la MAC
    printf("Enviando DHCPDISCOVER al servidor...\n");

    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, addr_len) < 0) {
        perror("Error al enviar DHCPDISCOVER");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Esperar respuesta del servidor (DHCPOFFER)
    if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr*)&server_addr, &addr_len) < 0) {
        perror("Error al recibir respuesta del servidor");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Recibido DHCPOFFER: %s\n", response.requested_ip);
    print_dhcp_options(&response);
    strcpy(assigned_ip, response.requested_ip); // Guardar la IP asignada

    // Enviar mensaje DHCPREQUEST para solicitar la IP ofrecida
    msg.message_type = 3; // 3 = DHCPREQUEST
    strcpy(msg.requested_ip, response.requested_ip);
    printf("Enviando DHCPREQUEST al servidor para la IP %s...\n", msg.requested_ip);

    if (sendto(sockfd, &msg, sizeof(msg), 0, (struct sockaddr*)&server_addr, addr_len) < 0) {
        perror("Error al enviar DHCPREQUEST");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    // Esperar respuesta del servidor (DHCPACK)
    if (recvfrom(sockfd, &response, sizeof(response), 0, (struct sockaddr*)&server_addr, &addr_len) < 0) {
        perror("Error al recibir DHCPACK");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    printf("Recibido DHCPACK: IP asignada %s\n", response.requested_ip);
    print_dhcp_options(&response);

    // Temporizador para la renovación
    int renew_interval = time_to_renew();
    printf("Renovación programada para %d segundos\n", renew_interval);

    // Intentar renovar la IP hasta un máximo de 4 veces
    while (renewals < MAX_RENEWALS) {
        sleep(renew_interval); // Simular espera hasta que llegue el tiempo de renovación
        renew_ip(sockfd, &server_addr, addr_len, "00:11:22:33:44:55", response.requested_ip);

        int renewal_status = handle_renewal_response(sockfd, &server_addr, addr_len);
        if (renewal_status == 1) {
            renewals++;
            printf("Renovación #%d exitosa\n", renewals);
            renew_interval = time_to_renew();  // Volver a programar la próxima renovación
        } else if (renewal_status == 0) {
            printf("Renovación fallida, se ha recibido un DHCPNAK. Fin del proceso.\n");
            break;
        } else {
            printf("Error en la renovación. Fin del proceso.\n");
            break;
        }
    }

    if (renewals == MAX_RENEWALS) {
        printf("Se ha alcanzado el límite de %d renovaciones.\n", MAX_RENEWALS);
    }

    // Cerrar el socket y enviar el DHCPRELEASE
    send_dhcp_release();
    close(sockfd);
    return 0;
}
