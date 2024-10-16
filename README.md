INTRODUCCION:
Proyecto DHCP: Implementación de Servidor y Cliente
Este proyecto tiene como objetivo implementar un servidor DHCP en C capaz de comunicarse con un cliente y asignar direcciones IP a clientes que soliciten configuraciones de red. El cliente, se encarga de solicitar una dirección IP al servidor usando el protocolo DHCP. La solución se desarrolla utilizando la API de Berkeley Sockets para la comunicación en red, y soporta funcionalidades clave del protocolo DHCP, tales como asignación dinámica de IPs

DESARROLLO:
El servidor DHCP está implementado en C y sigue el proceso básico de asignación dinámica de IPs a clientes conectados a la red, este servidor cuenta con un pool de direcciones IP, el cual se inicializa con un rango de direcciones IP previamente seleccionadas y especificas, estas IP se asignan dinámicamente a los clientes que lo solicitan, pues además el servidor está diseñado para manejar múltiples clientes simultáneamente, lo que permite responder a diferentes conexiones en paralelo.
Las funcionalidades principales del servidor son:
-	Asignación dinámica de IPs dentro de un rango configurado
-	Manejo de solicitudes (DHCDISCOVER, DHCPREQUEST y DHCPRELEASE)
-	Envió de las opciones de red básicas: mascara de subred, puerta de enlace, DNS y nombre de dominio

El cliente DHCP también esta desarrollado en C y se conecta al servidor para solicitar una dirección IP, el proceso empieza con la solicitud de la IP (DHCPDISCOVER), aquí el cliente envia una solicitud al servidor esperando recibir una dirección IP en respuesta.
Seguidamente la recepción de la IP (DHCPOFFER) se da cuando el cliente recibe un mensaje DHCPOFFER del servidor que incluye la IP ofrecida junto con las configuraciones adicionales
El cliente envia un mensaje de DHCPREQUEST para confirmar que acpeta la IP ofrecida.
Y finalmente el cliente recibe la confirmación mediante un mensaje DHCPACK lo cual significa que ya puede hacer uso de la IP asignada

ASPECTOS LOGRADOS Y NO LOGRADOS:
-	Implementación del servidor DHCP en C para asignar direcciones IP d

INSTRUCCIONES DE USO:

CONCLUSION:
Este proyecto permitió profundizar en el protocolo DHCP y su implementación mediante la API de sockets. Se desarrolló un servidor DHCP robusto, capaz de manejar múltiples clientes y gestionar concesiones de IP de forma eficiente. La implementación del cliente permitió simular el comportamiento real de un dispositivo que solicita configuraciones de red a un servidor

REFERENCIAS:
