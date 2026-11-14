# API de Conversión Monetaria

Documentación de referencia de la API utilizada para obtener los tipos de cambio diarios según el enunciado del curso y su especificación OpenAPI.

- **Documentación interactiva (Swagger UI):** https://api.sebastian.cl/cpyd/swagger-ui/index.html
- **Especificación OpenAPI:** https://api.sebastian.cl/cpyd/v3/api-docs
- **URL base:** `https://api.sebastian.cl/cpyd/v1`

---

## 1. Autenticación

La API requiere autenticación mediante token **JWT** (Bearer Token), con una validez de **67 minutos**.

### Endpoint

```http
POST https://api.sebastian.cl/cpyd/v1/login/authenticate
Content-Type: application/json
accept: application/json
```

### Cuerpo de la solicitud

Debe enviarse el correo institucional UTEM y el RUT con puntos y dígito verificador:

```json
{
  "email": "estudiante@utem.cl",
  "apiKey": "12.345.678-5"
}
```

### Respuesta

La respuesta JSON contiene el campo `jwt`, el cual debe enviarse en el encabezado `Authorization: Bearer <JWT>` en las consultas posteriores.

### Configuración segura de credenciales

El token JWT **no** debe quedar hardcodeado en el código fuente ni publicarse en el repositorio. El programa obtiene las credenciales desde variables de entorno (o archivo `.env` ignorado por Git, ver [`.env.example`](../.env.example)):

- `CPYD_EMAIL`: correo institucional UTEM.
- `CPYD_APIKEY`: RUT con puntos y guion.
- `CPYD_API_URL` (opcional): URL base de la API (por defecto `https://api.sebastian.cl/cpyd/v1`).

---

## 2. Consulta de tipos de cambio por rango de fechas (optimización principal)

Permite obtener todas las cotizaciones de un país entre `start_date` y `end_date` (ambas inclusive, máximo 3.660 días) en **una sola petición HTTP**. De este modo, todo el período `2023-01-01` al `2025-12-31` se descarga en **3 peticiones en total** (una por país: `chile`, `colombia` y `ecuador`).

### Endpoint

```http
GET https://api.sebastian.cl/cpyd/v1/exchanges/{country}?start_date=2023-01-01&end_date=2025-12-31
Authorization: Bearer <JWT>
accept: application/json
```

### Parámetros

| Parámetro | Ubicación | Valores permitidos | Descripción |
|---|---|---|---|
| `country` | `path` | `chile`, `colombia`, `ecuador` | País en minúsculas (`0` → `chile`, `1` → `colombia`, `2` → `ecuador`) |
| `start_date` | `query` | `YYYY-MM-DD` | Fecha inicial inclusive (`2023-01-01`) |
| `end_date` | `query` | `YYYY-MM-DD` | Fecha final inclusive (`2025-12-31`) |

### Ejemplo de respuesta

Devuelve un arreglo JSON con una cotización por cada día hábil con dato en el rango:

```json
[
  {
    "country": "Chile",
    "date": "2023-01-02",
    "rate": 850.5
  },
  {
    "country": "Chile",
    "date": "2023-01-03",
    "rate": 855.2
  }
]
```

> **Nota sobre días sin cotización:** Yahoo Finance solo publica datos en días hábiles; los fines de semana y feriados no aparecen en la lista. En Ecuador la moneda local es el dólar estadounidense, por lo que su tasa siempre vale `1.00`.

---

## 3. Consulta de tipo de cambio de un día específico

### Endpoint

```http
GET https://api.sebastian.cl/cpyd/v1/exchanges/{country}/{date}
Authorization: Bearer <JWT>
accept: application/json
```

### Ejemplo de respuesta

```json
{
  "country": "Chile",
  "date": "2025-06-20",
  "rate": 940.58
}
```

---

## 4. Conversión monetaria a USD

El campo `rate` representa la cantidad de moneda local equivalente a `1 USD`. La conversión debe realizarse como:

$$\text{monto\_USD} = \frac{\text{monto\_local}}{\text{rate}}$$

---

## 5. Códigos de estado HTTP y manejo de errores

| Código HTTP | Significado en la API | Acción del programa |
|---|---|---|
| `200` | Consulta exitosa | Parsear JSON y poblar caché de tasas |
| `400` | Parámetro inválido (país o fecha mal formada) | Abortar e informar error de formato |
| `401` | Credenciales inválidas en login | Informar error de autenticación |
| `403` | Token JWT ausente, inválido o expirado | **Renovar el token JWT** mediante `POST /login/authenticate` en sección crítica (`#pragma omp critical`) y reintentar la petición |
| `404` | Sin datos para la consulta (fines de semana/feriados) | Aplicar política de respaldo: usar la última tasa disponible anterior del mismo país (o la primera posterior al inicio del período) |
| `429` | Demasiados intentos fallidos de inicio de sesión | Informar bloqueo temporal (15 minutos) |
