import { Component } from '@angular/core';
import { HttpClient, HttpParams } from '@angular/common/http';

@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})
export class AppComponent {
  title = 'ExtreamFS';
  salida: string = '';

  constructor(private http: HttpClient) {}

  // ✅ Función para ejecutar un comando y ESPERAR la respuesta
  ejecutarComandoAsync(comando: string): Promise<string> {
    return new Promise((resolve, reject) => {
      const params = new HttpParams()
        .set('comando', comando);

      this.http.post('http://localhost:8080/api/comando', 
        params.toString(),
        { 
          responseType: 'text',
          headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
        }
      ).subscribe({
        next: (respuesta) => {
          resolve(respuesta);
        },
        error: (err) => {
          resolve('Error: ' + err.message);
        }
      });
    });
  }

  // ✅ Ejecución SECUENCIAL con async/await
  async ejecutarComandos() {
    const inputElement = document.getElementById('inputComandos') as HTMLTextAreaElement;
    const input = inputElement ? inputElement.value : '';
    
    if (!input || !input.trim()) {
      this.salida = "Error: No hay comandos para ejecutar.\n";
      return;
    }

    this.salida = '';
    const lineas = input.split('\n');

    console.log('=== DEBUG: Ejecución Secuencial ===');
    console.log('Total líneas:', lineas.length);

    // ✅ Ejecutar línea por línea SECUENCIALMENTE
    for (const linea of lineas) {
      const lineaTrim = linea.trim();

      console.log(`Procesando: "${lineaTrim}"`);

      // Si es línea en blanco o comentario, solo mostrarla (NO ejecutar)
      if (lineaTrim === '' || lineaTrim.startsWith('#')) {
        this.salida += linea + '\n';
        console.log('  → Skip (comentario/vacío)');
        continue;  // ✅ Continuar con la siguiente línea
      }

      // ✅ Ejecutar comando y ESPERAR la respuesta antes de continuar
      this.salida += linea + '\n';
      const respuesta = await this.ejecutarComandoAsync(lineaTrim);
      this.salida += respuesta + '\n';
      
      console.log('  → Respuesta:', respuesta.substring(0, 50) + '...');
      
      // ✅ Pequeña pausa entre comandos (opcional, ayuda a estabilidad)
      await new Promise(resolve => setTimeout(resolve, 100));
    }

    console.log('=== FIN Ejecución ===');
  }

  cargarScript() {
    const fileInput = document.getElementById('fileInput') as HTMLInputElement;
    if (fileInput) {
      fileInput.click();
    }
  }

  onFileSelected(event: any) {
    const file = event.target.files[0];
    if (file) {
      const reader = new FileReader();
      reader.onload = (e: any) => {
        const inputElement = document.getElementById('inputComandos') as HTMLTextAreaElement;
        if (inputElement) {
          inputElement.value = e.target.result;
        }
      };
      reader.readAsText(file);
    }
  }
}