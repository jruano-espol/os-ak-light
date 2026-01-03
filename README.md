# AK Light

## Avance Práctica 3

Tomando de referencia el proyecto final. La práctica será realizar un primer avance considerando la implementación de un broker, productor, consumidor y un tópico con varios niveles.
- `tópico_general/subtópico1/subtópico2`

Para los datos a sensar se va manejar indicadores como uso de memoria, espacio en disco, etc. 

La implementación de tanto los brokers como los productores deberán estar en containers (dockers)

Entregables:
- Código en C
- Informe (Capturas y explicación de lo realizado + diagrama de la solución)
- Video (5min explicación código + 5min demo)

## Instrucciones

Se requiere desarrollar y desplegar una solución de monitoreo de contenedores (Docker) para
conocer el estado de los mismos, para ello deberá desarrollar una versión ligera de la solución
Apache Kafka, considerando lo siguiente:

### Broker
El broker se denomina a un servicio que estará disponible para atender mensajes de diferentes
productores. El mismo gestionará una versión mejorada de cola de mensajes tradicionalmente
utilizado en soluciones de MQTT, considerando la persistencia de los mismos. Para identificar
cada mensaje se asociará un tópico.

### Productor
Se denomina a un sistema que desea enviar un mensaje a un broker para que pueda ser
difundido a diferentes consumidores. El mensaje será identificado mediante un tópico.

### Consumidor
Se denomina a un sistema que desea recibir mensajes de uno o varios productores mediante un
broker.

### Tópico
Es una forma para identificar a los mensajes. Los productores envían mensajes a un tópico
específico y los consumidores se suscriben a los tópicos para recibir los mensajes. Los tópicos
deberán ser considerados de diferentes niveles. Los mensajes que se envían deberán ser
almacenados en un broker por un periodo de tiempo configurable, permitiendo que los
consumidores tengan acceso a los datos históricos de ser necesario.

### Partición
Cada tópico puede dividirse en una o más particiones y cada partición se almacena en uno o
más brokers.

### Clúster
Se denomina a la agrupación de varios brokers con la finalidad de gestionar los mensajes de los
tópicos que se encuentren particionados.

### Funcionamiento
Para el funcionamiento de la solución tomen en consideración lo siguiente:

- Los productores deben enviar mensajes a los brokers, para esto se deberá definir un
tópico y una clave (La misma determinará en que partición se almacenará el mensaje. En
el caso de no establecerse la clave deberá implementar un mecanismo de round-robin
donde cada partición va recibir un mensaje de manera cíclica con la finalidad de
distribuir los mensajes en las diferentes particiones.

- Los tópicos deberán ser considerados de N niveles. Por ejemplo:
    - `tópico_general/subtópico1/subtópico2`

- Los tópicos deberán considerar un wildcard de múltiples niveles. Por ejemplo:
    - `tópico/subtópico1/#`

- Para el caso de los consumidores deberá implementarse la posibilidad de que se maneje una sesión (persistente y no persistente).

### Despliegue de la solución
Para el despliegue de la solución deberá considerar lo siguiente:

- Deberá implementar mínimo 5 contenedores (Docker), los cuales 2 serán de tipo broker
y conformarán un clúster, otros 2 serán de tipo productor y 1 será de tipo consumidor.

- Deberá implementar la conectividad entre los diferentes contenedores.

- Cada productor deberá estar en la capacidad de enviar mínimo 2 métricas del contenedor. Por ejemplo:
    - Uso de la cpu
    - Memoria
    - Disco
    - Latencia
    - Número de errores en logs
    - etc.

- Deberá utilizar tópicos con mínimo 2 niveles, wildcard de múltiple nivel.

- Deberá implementar que la sesión pueda ser persistente y no persistente, y que sea configurada inicialmente.
