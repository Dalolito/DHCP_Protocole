PROYECYO DHCP: IMPLEMENTACION DE SERVIDOR Y CLIENTE

INTRODUCCION:

Este proyecto tiene como objetivo implementar un servidor DHCP en C capaz de asignar direcciones IP a los clientes que soliciten configuraciones de red. El cliente se encarga de solicitar una dirección IP al servidor usando el protocolo DHCP. La solución se desarrolla utilizando la API de Berkeley Sockets para la comunicación en red, y soporta funcionalidades clave del protocolo DHCP, como la asignación dinámica de IPs, gestión de concesiones (lease), y liberación de direcciones.

DESARROLLO:

Servidor DHCP

El servidor DHCP está implementado en C y sigue el proceso básico de asignación dinámica de IPs a clientes conectados a la red. El servidor cuenta con un pool de direcciones IP, el cual se inicializa con un rango previamente seleccionado. Estas IPs se asignan dinámicamente a los clientes que lo solicitan. Además, el servidor está diseñado para manejar múltiples clientes simultáneamente, permitiendo atender diversas conexiones en paralelo.

Funcionalidades principales del servidor:
- Asignación dinámica de direcciones IP dentro de un rango configurado.
- Manejo de solicitudes DHCP como DHCPDISCOVER, DHCPREQUEST y DHCPRELEASE.
- Envío de opciones de red básicas: máscara de subred, puerta de enlace predeterminada, DNS y nombre de dominio.
  
Cliente DHCP

El cliente DHCP también está desarrollado en C y se conecta al servidor para solicitar una dirección IP. El proceso incluye varias etapas:

- Solicitud de IP (DHCPDISCOVER): El cliente envía una solicitud al servidor para obtener una dirección IP.
- Recepción de la IP (DHCPOFFER): El servidor responde al cliente con una oferta que incluye la dirección IP junto con configuraciones adicionales de red.
- Confirmación de la IP (DHCPREQUEST): El cliente envía un mensaje de confirmación aceptando la IP ofrecida.
- Asignación final de la IP (DHCPACK): El servidor confirma la asignación, lo que permite al cliente comenzar a usar la IP.
  
Relay DHCP

Se ha añadido la funcionalidad de Relay DHCP para permitir que los clientes en diferentes subredes se comuniquen con el servidor DHCP central. El Relay Agent actúa como intermediario, recibiendo las solicitudes DHCP de los clientes en una subred y reenviándolas al servidor DHCP, que podría estar en una subred diferente.

El Relay Agent escucha en un puerto dedicado para las solicitudes de los clientes. Cuando recibe un mensaje, agrega su dirección IP en el campo giaddr (gateway IP address) del mensaje DHCP antes de reenviarlo al servidor. De esta manera, el servidor puede identificar la subred de origen del cliente y asignar una IP adecuada. Posteriormente, el relay recibe la respuesta del servidor y la reenvía al cliente.

ASPECTOS LOGRADOS Y NO LOGRADOS:

Aspectos logrados:

- Implementación completa del servidor DHCP en C, capaz de asignar direcciones IP dinámicamente a los clientes.
- Manejo correcto de múltiples solicitudes de clientes simultáneos mediante el uso de sockets y concurrencia.
- Soporte para las fases principales del protocolo DHCP: DHCPDISCOVER, DHCPOFFER, DHCPREQUEST y DHCPACK.
- Envío de las opciones de red necesarias, como máscara de subred, puerta de enlace predeterminada, servidor DNS y nombre de dominio.
- Correcta gestión del tiempo de concesión (lease) de las IPs asignadas, incluyendo la renovación y liberación de direcciones IP cuando sea necesario.
- Control de las direcciones IP asignadas y liberadas, registrando las concesiones de forma precisa.

Aspectos no logrados:

- No se ha logrado implementar la funcionalidad de comunicación entre subredes usando DHCP de manera completa.
- No se ha logrado implementar la aplicación en un servidor en la nube, y el cliente se ejecutó en la misma subred que el servidor.

CONCLUSIONES:

REFERENCIAS:

Este proyecto permitió profundizar en el protocolo DHCP y su implementación mediante la API de sockets. Se desarrolló un servidor DHCP robusto, capaz de manejar múltiples clientes y gestionar concesiones de IP de forma eficiente. La implementación del cliente permitió simular el comportamiento real de un dispositivo que solicita configuraciones de red a un servidor.

https://www.cisco.com/c/en/us/td/docs/routers/ncs4200/configuration/guide/IP/17-1-1/b-dhcp-17-1-1-ncs4200/b-dhcp-17-1-1-ncs4200_chapter_00.pdf
