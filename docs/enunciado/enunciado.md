

**Facultad de Ingeniería.** 

**Departamento de Informática y Computación.** 

**Académico: Sebastián Salazar Molina.** 



<!-- Start of picture text -->
UTEM<br>UNIVERSIDAD TECNOLOGICA<br>METROPOLITANA<br>del Estado de Chile<br><!-- End of picture text -->

**Procesamiento paralelo con OpenMP de ventas multinacionales y conversión monetaria a USD mediante API** 

**14 de noviembre de 2026** 

# **ÍNDICE** 

|**ÍNDICE....................................................................................................................................................... 1**|
|---|
|**Contexto....................................................................................................................................... 2**|
|Objetivo general.......................................................................................................................2|
|Objetivos específicos.........................................................................................................2|
|Datos de entrada..................................................................................................................... 3|
|**Requisitos funcionales............................................................................................................... 4**|
|API de conversión monetaria...................................................................................................5|
|Autenticación:.................................................................................................................... 5|
|Consulta de tipo de cambio:.............................................................................................. 5|
|**Requisitos de paralelismo con OpenMP................................................................................... 7**|
|**Entregables.................................................................................................................................. 7**|
|**Criterios de evaluación............................................................................................................... 8**|
|**Plazo de entrega.......................................................................................................................... 8**|
|**Observaciones académicas........................................................................................................9**|



1 

# **Contexto** 

Una multinacional ha adquirido tres empresas del mismo rubro en Chile, Colombia y Ecuador. La gerencia necesita conocer las ventas de estas empresas expresadas en dólares estadounidenses (USD), considerando que cada país registra sus ventas en su moneda local. Para ello, se dispone de un archivo de transacciones y de una API que entrega el tipo de cambio diario. 

Se solicita desarrollar una aplicación en C/C++ (Que compile en **GCC 14.2.0** ) que utilice paralelismo con OpenMP para procesar las ventas del período comprendido entre el 01/01/2023 y el 31/12/2025, convertir los montos locales a USD según la fecha de cada venta y registrar la suma de ventas por identificador de cliente, agrupada por país. 

# **Objetivo general** 

Diseñar e implementar una solución paralela en C/C++ con OpenMP que procese transacciones de ventas de Chile, Colombia y Ecuador, consulte los tipos de cambio diarios a través de una API autenticada y genere un resumen de ventas en USD por cliente y país. 

## **Objetivos específicos** 

- Leer y validar un archivo de transacciones con ventas de tres países. 

- Filtrar las ventas cuyo rango de fecha esté entre el 01/01/2023 y el 31/12/2025, inclusive. 

- Obtener un token JWT mediante el endpoint de autenticación de la API. 

- Consultar el tipo de cambio diario por país y fecha. 

- Convertir cada monto local a USD usando la tasa correspondiente al día de la venta. 

- Acumular correctamente las ventas por cliente y país, evitando condiciones de carrera. 

- Generar un archivo de salida con los totales en USD. 

2 

# **Datos de entrada** 

El archivo de transacciones contiene una línea por venta, con los siguientes campos en orden: 

|**Campo**|**Tipo**|**Descripción**|
|---|---|---|
|País|Entero|0 = Chile, 1 = Colombia, 2 =<br>Ecuador|
|Identificador de cliente|UUID|Este valor representa un único<br>cliente en cada país, un cliente<br>puede tener muchas compras y<br>un identificador se puede repetir<br>en otro país|
|Identificador de boleta|UUID|Valor único en todo el archivo; no<br>puede repetirse|
|Fecha y hora local de la<br>venta|Fecha y Hora|Fecha/hora del registro|
|Monto de la venta|Decimal|Expresado en moneda local del<br>país|



El programa debe validar, al menos: 

- País válido (0, 1 o 2). 

- Identificador de cliente válido. 

- Boleta no duplicada en todo el archivo. 

- Fecha dentro del rango solicitado. 

- Monto numérico positivo. 

- Formato de fecha/hora consistente. 

Si se detecta un registro inválido o una boleta duplicada, el programa debe informar claramente la incidencia y aplicar una política (Por ejemplo: registrar la fila en un archivo de errores e ignorar dicha fila del procesamiento) que cada equipo deberá definir y explicitar. 

3 

# **Requisitos funcionales** 

1. Filtrado temporal: procesar únicamente ventas entre el 01/01/2023 y el 31/12/2025, inclusive. 

2. Conversión monetaria: para cada venta, consultar la tasa de cambio del país y la fecha de la venta. La conversión debe realizarse como: monto_USD = monto_local / rate, donde rate corresponde a la cantidad de moneda local por 1 USD entregada por la API. 

3. Agrupación: acumular la suma de ventas en USD para cada par (país, identificador_cliente). 

4. Salida: generar un archivo CSV, por ejemplo resumen_ventas_usd.csv, con columnas: 

None 

pais,id_cliente,total_usd 

Los resultados deben estar ordenados por país y luego por identificador de cliente. 

5. Optimización: se espera la implementación de mecanismo que permitan acelerar la ejecución del programa. 

6. Autenticación: obtener el token JWT una vez al inicio y reutilizarlo en las consultas. El token no debe quedar hardcodeado en el código ni en el repositorio. 

7. Manejo de errores: controlar errores HTTP, timeouts, respuestas inválidas, token expirado y fechas sin tasa disponible. Si la API no entrega una tasa para una fecha, el programa debe registrar la situación y aplicar una política documentada, por ejemplo, usar la última tasa disponible anterior o promediar entre las fechas que existan registros. 

4 

# **API de conversión monetaria** 

La API requiere autenticación. 

## **Autenticación:** 

None POST https://api.sebastian.cl/cpyd/v1/login/authenticate Content-Type: application/json accept: application/json 

Cuerpo de ejemplo: 

JSON { "email": "estudiante@utem.cl", "apiKey": "12.345.678-5" } 

Debe usar su correo de la universidad y su propio rut con puntos y dígito verificador. La respuesta contiene un campo jwt que debe usarse como Bearer Token. 

## **Consulta de tipo de cambio:** 

None GET https://api.sebastian.cl/cpyd/v1/exchanges/{country}/{date} Authorization: Bearer <JWT> accept: application/json 

5 

Donde: 

- country: chile, colombia o ecuador. 

- date: fecha en formato YYYY-MM-DD. 

Ejemplo de respuesta: 

JSON { "country": "Chile", "date": "2025-06-20", "rate": 940.58 } 

El programa debe mapear correctamente los códigos del archivo: 

- 0 → Chile 

- 1 → Colombia 

- 2 → Ecuador 

No se deben hardcodear tasas de cambio para ningún país. 

6 

# **Requisitos de paralelismo con OpenMP** 

La solución debe usar OpenMP de forma efectiva y justificada. Se acepta, por ejemplo: 

- Paralelizar el procesamiento de registros o particiones por país. 

- Paralelizar la consulta/obtención de tasas con una caché sincronizada. 

- Utilizar reducciones, secciones críticas, operaciones atómicas o acumuladores por hilo para evitar condiciones de carrera. 

La solución debe: 

1. Compilar con g++ o gcc usando -fopenmp. 

2. Evitar condiciones de carrera en el programa. 

3. Documentar la estrategia de paralelización utilizada. 

4. Incluir una comparación de tiempos usando distintos números de hilos. 

5. Presentar un análisis básico de speedup y eficiencia. 

No basta con incluir directivas OpenMP: se evaluará que el paralelismo sea correcto, útil y coherente con el problema. 

# **Entregables** 

El proyecto debe entregarse en un repositorio GitHub. Todos los integrantes del grupo deben realizar aportes verificables mediante commits. Se debe agregar como miembro del proyecto al docente con cuenta: 

None https://github.com/ **sebasalazar** 

El repositorio debe incluir: 

1. Código fuente en C/C++. 

2. Archivo README.md con instrucciones de compilación y ejecución. 

3. Descripción del formato de entrada utilizado. 

4. Explicación de la estrategia de paralelización. 

5. Política de manejo de errores y de fechas sin tasa disponible. 

7 

6. Resultados de ejemplo y análisis de rendimiento. 

7. Instrucciones para configurar credenciales sin exponer tokens reales. 

8. Registro de participación del grupo. 

La tarea puede realizarse de forma individual, en parejas o en grupos de máximo 3 estudiantes. 

# **Criterios de evaluación** 

Se evaluará: 

- Correctitud funcional del procesamiento y de la conversión a USD. 

- Uso correcto y justificado de OpenMP. 

- Ausencia de condiciones de carrera. 

- Calidad del código, modularidad y documentación. 

- Manejo de errores y validaciones. 

- Uso adecuado de la API, impementación de optimizaciones. 

- Análisis de rendimiento secuencial versus paralelo. 

- Participación verificable de todos los integrantes en GitHub. 

# **Plazo de entrega** 

La entrega máxima será el **14/11/2026** a las **12:30 horas** , hora continental de Chile. 

8 

# **Observaciones académicas** 

- No se permite hardcodear resultados, tasas de cambio ni tokens. 

- El token JWT debe manejarse de forma segura y no debe publicarse en el repositorio. 

- Se recomienda usar libcurl para las llamadas HTTP y una biblioteca de parsing JSON compatible con C/C++. 

- El programa debe poder ejecutarse desde línea de comandos, por ejemplo: 

Shell 

./programa --input ventas.csv --output resumen_ventas_usd.csv --threads 8 

- Cualquier supuesto adicional debe quedar documentado en el README.md. 

9 

