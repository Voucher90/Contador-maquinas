# Contador de Ciclos y RPM para Máquinas Rotativas

Contador de ciclos y RPM para máquinas rotativas con salida de relé para parada por número predefinido.

## Características Principales

* **Conteo Preciso:** Mide tanto los ciclos completados como las RPM en tiempo real.
* **Parada Automática:** Detiene la máquina de forma segura al alcanzar un número de ciclos predefinido (consigna).
* **Memoria Persistente (EEPROM):** Guarda la consigna y el conteo de ciclos actual, recuperando el estado del ensayo incluso después de un corte de energía. Incluye un sistema de gestión para proteger la vida útil de la memoria.
* **Configurable:** Permite ajustar el comportamiento del relé de parada para contactos normalmente abiertos (NA) o normalmente cerrados (NC).
* 
* **Versiones Adaptadas:** El proyecto cuenta con ramas específicas para:
    * Una versión optimizada que interactúa directamente con los registros del microcontrolador.
    * Una adaptación para una máquina de ensayos de velcro que controla el giro del motor.

## Uso Básico

1.  **Cargar el programa** en la placa Arduino.
2.  **Configurar la consigna** deseada utilizando los pulsadores. La configuración se guardará automáticamente.
3.  **Iniciar el ensayo**. El contador empezará a registrar los ciclos y las RPM.
4.  La máquina se detendrá al alcanzar el número de ciclos de la consigna.
