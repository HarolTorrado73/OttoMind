# 5. Emojis: por qué son “estáticos” y cómo animarlos

## Comportamiento en código — `LcdDisplay::SetEmotion`

Archivo: `main/display/lcd_display.cc`.

1. Pide a la colección del tema actual: `emoji_collection()->GetEmojiImage(emotion)`.
2. Si **no** hay imagen para ese nombre: intenta **Font Awesome** UTF-8 en `emoji_label_` (también estático).
3. Si hay imagen:
   - Si `image->IsGif()` es **true** → crea `LvglGif`, callback de frame, `Start()` → **animación GIF**.
   - Si es **false** → `lv_image_set_src` una sola vez → **fija** (PNG/bin embebido).

## Por qué otto-pet-custom muestra emojis fijos

En `main/CMakeLists.txt`, para `CONFIG_BOARD_TYPE_OTTO_PET_CUSTOM`:

```cmake
set(DEFAULT_EMOJI_COLLECTION twemoji_32)
```

`Twemoji32` / `Twemoji64` en `emoji_collection.cc` mapean nombres lógicos (`happy`, `sad`, …) a **`lv_image_dsc_t` estáticos** (mapas de bits), **no** a GIF.

La placa **otto-robot** usa en cambio:

```cmake
set(DEFAULT_EMOJI_COLLECTION otto-gif)
```

y además una subclase `OttoEmojiDisplay` con flujo Otto + assets; ahí el ecosistema está pensado más para **GIF / packs Otto**.

## Cómo pasar a emojis animados (opciones)

1. **Cambiar colección por defecto** en `CMakeLists.txt` de `twemoji_32` a **`otto-gif`** (como `otto-robot`) y recompilar: más peso de assets y conviene verificar que los nombres de emociones que envía el servidor coincidan con los del pack.
2. **OTA / `assets`**: si el JSON de configuración de assets incluye `emoji_collection` como array de `{ name, file }` apuntando a **archivos GIF** en el paquete, `Assets` en `assets.cc` construye un `EmojiCollection` con `LvglRawImage`; `IsGif()` detecta GIF y `SetEmotion` anima.
3. **Pantalla tipo OttoEmojiDisplay**: implica acercar el comportamiento de `otto-robot` a `otto-pet-custom` (más trabajo de integración y pruebas).

## Resumen

Los emojis “estáticos” **no son un fallo del IDF**: es la **colección Twemoji por defecto** elegida para `otto-pet-custom`. La animación ya está implementada en `LcdDisplay`; hace falta **origen de imagen GIF** (assets u `otto-gif`).
