FROM nginx:latest

# Kopiere die HTML-Datei und die .dzi-Dateien in den Webserver
COPY viewer.html /usr/share/nginx/html/index.html
COPY deepzoom/ /usr/share/nginx/html/deepzoom/

# Exponiere den Standardport
EXPOSE 80
