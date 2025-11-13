#include <iostream>
#include <vector>
#include <windows.h>
#include <conio.h>
#include <ctime>
#include <cstdlib>
#include <thread>
#include <chrono>

using namespace std;

// Dimensiones del laberinto
const int WIDTH = 19;
const int HEIGHT = 19;

// Caracteres del laberinto
const char PARED = '#';
const char PUNTO = '.';
const char VACIO = ' ';
const char PACMAN = 'C';
const char FANTASMA = 'F';
const char POWER_PELLET = 'O';

// Colores de consola (Windows) - neon blue como bright cyan
const WORD COLOR_NEON_BLUE = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;

// Direcciones
enum Direccion { ARRIBA, ABAJO, IZQUIERDA, DERECHA, NINGUNA };

// Estructura para posición
struct Posicion {
    int x, y;
    Posicion(int x = 0, int y = 0) : x(x), y(y) {}
};

class PacmanGame {
private:
    vector<vector<char>> laberinto;
    Posicion pacman;
    vector<Posicion> fantasmas;
    vector<Direccion> direccionesFantasmas;
    int puntuacion;
    int vidas;
    bool juegoActivo;
    bool powerMode;
    int powerTimer;
    int puntosRestantes;

    // Cuenta cuántas vidas extra ya se han otorgado por alcanzar múltiplos de 10000 puntos
    int extraLivesAwarded;

    // Laberinto inicial (19x19) — pared derecha corregida, atajos desbloqueados según mapa original
    vector<string> laberintoBase = {
        "###################",
        "#.................#",
        "#.###.#.#.#.###.#.#",
        "#O###.#.#.#.###.#O#",
        "#.###.#.#.#.###.#.#",
        "#.................#",
        "#.###.#.###.#.###.#",
        "#.....#.....#.....#",
        "#####.# ##### #.###",
        "#    .#     #  .  #",
        "#####.# ##### #.###",
        "#................ #",
        "#.###.#.#.#.###.#.#",
        "#O..#.#...#.#....O#",
        "#.#.#.#.###.#.#.###",
        "#...#.....#.......#",
        "#.#########.#####.#",
        "#.................#",
        "###################"
    };

    void inicializarLaberinto() {
        // Asignar laberinto con valores por defecto (pared)
        laberinto.assign(HEIGHT, vector<char>(WIDTH, PARED));
        puntosRestantes = 0;

        for (int y = 0; y < HEIGHT; y++) {
            const string& row = (y < static_cast<int>(laberintoBase.size())) ? laberintoBase[y] : string();
            for (int x = 0; x < WIDTH; x++) {
                // Si falta carácter en la fila, usar VACIO (pasillo) en lugar de PARED para evitar bloquear movimiento
                char c = (x < static_cast<int>(row.size())) ? row[x] : VACIO;
                laberinto[y][x] = c;
                if (laberinto[y][x] == PUNTO || laberinto[y][x] == POWER_PELLET) {
                    puntosRestantes++;
                }
                // NOTA: no asignamos pacman aquí para evitar que vuelva a colocarlo múltiples veces.
            }
        }
    }

    void inicializarFantasmas() {
        // Posiciones iniciales de los fantasmas
        fantasmas = {
            Posicion(9, 9),
            Posicion(8, 9),
            Posicion(10, 9),
            Posicion(9, 8)
        };

        direccionesFantasmas = { DERECHA, IZQUIERDA, ARRIBA, ABAJO };
    }

    bool esPosicionValida(int x, int y) {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return false;
        // Permitir moverse a cualquier celda que no sea pared.
        return laberinto[y][x] != PARED;
    }

    // Centraliza la suma de puntos y gestiona la concesión de vidas extra cada 10000 puntos.
    void addPuntos(int puntos) {
        if (puntos <= 0) return;
        puntuacion += puntos;

        // Calcular cuántas vidas extra corresponde otorgar según puntuación total
        int awardedNow = puntuacion / 10000;
        if (awardedNow > extraLivesAwarded) {
            int delta = awardedNow - extraLivesAwarded;
            vidas += delta;
            extraLivesAwarded = awardedNow;
            // (Opcional) mostrar feedback inmediato -- evitamos prints extra para no degradar rendimiento.
        }
    }

    void moverPacman(Direccion dir) {
        int nuevoX = pacman.x;
        int nuevoY = pacman.y;

        switch (dir) {
        case ARRIBA: nuevoY--; break;
        case ABAJO: nuevoY++; break;
        case IZQUIERDA: nuevoX--; break;
        case DERECHA: nuevoX++; break;
        default: break;
        }

        // Teletransporte entre túneles (mantener Y igual si fuera fuera de rango verticalmente)
        if (nuevoX < 0) nuevoX = WIDTH - 1;
        else if (nuevoX >= WIDTH) nuevoX = 0;

        if (esPosicionValida(nuevoX, nuevoY)) {
            pacman.x = nuevoX;
            pacman.y = nuevoY;
            recolectarPunto(nuevoX, nuevoY);
        }
    }

    void recolectarPunto(int x, int y) {
        if (laberinto[y][x] == PUNTO) {
            addPuntos(10);
            puntosRestantes--;
            laberinto[y][x] = VACIO;
        }
        else if (laberinto[y][x] == POWER_PELLET) {
            addPuntos(50);
            puntosRestantes--;
            laberinto[y][x] = VACIO;
            activarPowerMode();
        }

        if (puntosRestantes == 0) {
            // Nivel completado
            reiniciarNivel();
        }
    }

    void activarPowerMode() {
        powerMode = true;
        powerTimer = 200; // Duración del power mode
    }

    void actualizarPowerMode() {
        if (powerMode) {
            powerTimer--;
            if (powerTimer <= 0) {
                powerMode = false;
            }
        }
    }

    void moverFantasmas() {
        for (int i = 0; i < static_cast<int>(fantasmas.size()); i++) {
            // Movimiento simple semi-aleatorio
            int direccion = rand() % 10;
            if (direccion < 7) {
                // 70% de probabilidad de mantener dirección
                if (!intentarMoverFantasma(i, direccionesFantasmas[i])) {
                    direccionesFantasmas[i] = obtenerDireccionAleatoria(i);
                }
            }
            else {
                // 30% de probabilidad de cambiar dirección
                direccionesFantasmas[i] = obtenerDireccionAleatoria(i);
            }
        }
    }

    bool intentarMoverFantasma(int indice, Direccion dir) {
        int nuevoX = fantasmas[indice].x;
        int nuevoY = fantasmas[indice].y;

        switch (dir) {
        case ARRIBA: nuevoY--; break;
        case ABAJO: nuevoY++; break;
        case IZQUIERDA: nuevoX--; break;
        case DERECHA: nuevoX++; break;
        default: break;
        }

        // Teletransporte entre túneles
        if (nuevoX < 0) nuevoX = WIDTH - 1;
        else if (nuevoX >= WIDTH) nuevoX = 0;

        if (esPosicionValida(nuevoX, nuevoY)) {
            fantasmas[indice].x = nuevoX;
            fantasmas[indice].y = nuevoY;
            return true;
        }
        return false;
    }

    Direccion obtenerDireccionAleatoria(int indiceFantasma) {
        vector<Direccion> direccionesPosibles;
        int x = fantasmas[indiceFantasma].x;
        int y = fantasmas[indiceFantasma].y;

        if (esPosicionValida(x, y - 1)) direccionesPosibles.push_back(ARRIBA);
        if (esPosicionValida(x, y + 1)) direccionesPosibles.push_back(ABAJO);
        if (esPosicionValida(x - 1, y)) direccionesPosibles.push_back(IZQUIERDA);
        if (esPosicionValida(x + 1, y)) direccionesPosibles.push_back(DERECHA);

        if (direccionesPosibles.empty()) return NINGUNA;
        return direccionesPosibles[rand() % direccionesPosibles.size()];
    }

    void verificarColisiones() {
        for (int i = 0; i < static_cast<int>(fantasmas.size()); i++) {
            if (fantasmas[i].x == pacman.x && fantasmas[i].y == pacman.y) {
                if (powerMode) {
                    // Pacman se come al fantasma
                    addPuntos(200);
                    // Reubicar fantasma
                    fantasmas[i] = Posicion(9, 9);
                }
                else {
                    // Pacman pierde vida
                    vidas--;
                    if (vidas <= 0) {
                        juegoActivo = false;
                    }
                    else {
                        reiniciarPosiciones();
                    }
                }
                break;
            }
        }
    }

    void reiniciarPosiciones() {
        // Ubicar a Pac-Man en la posición inicial fija (una sola vez por reinicio de posiciones)
        pacman = Posicion(9, 15);
        inicializarFantasmas();
        powerMode = false;
    }

    void reiniciarNivel() {
        inicializarLaberinto();
        reiniciarPosiciones();
    }

public:
    PacmanGame() : puntuacion(0), vidas(3), juegoActivo(true), powerMode(false), powerTimer(0), extraLivesAwarded(0) {
        srand(static_cast<unsigned int>(time(nullptr)));
        inicializarLaberinto();
        reiniciarPosiciones(); // colocar pacman y fantasmas una sola vez al construir
    }

    // Nueva función: reinicia todo el estado del juego como si arrancara por primera vez.
    void reiniciarJuego() {
        puntuacion = 0;
        vidas = 3;
        extraLivesAwarded = 0;
        juegoActivo = true;
        powerMode = false;
        powerTimer = 0;
        inicializarLaberinto();   // restaura el mapa y recalcula puntosRestantes
        reiniciarPosiciones();    // coloca pacman y fantasmas en posiciones iniciales
    }

    void dibujar() {
        // Evitar system("cls") por ser lento: reposicionar cursor al origen para sobrescribir pantalla
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        COORD origin = { 0, 0 };
        SetConsoleCursorPosition(hConsole, origin);

        // Imprimir cabecera
        cout << "PACMAN - Puntuacion: " << puntuacion << " - Vidas: " << vidas;
        if (powerMode) cout << " - POWER MODE!";
        cout << "                              " << endl; // padding para limpiar restos
        // Dibujar laberinto y entidades
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                bool hayFantasma = false;
                for (const auto& fantasma : fantasmas) {
                    if (fantasma.x == x && fantasma.y == y) {
                        if (powerMode) {
                            SetConsoleTextAttribute(hConsole, 11); // Azul claro
                            cout << FANTASMA;
                        }
                        else {
                            SetConsoleTextAttribute(hConsole, 12); // Rojo
                            cout << FANTASMA;
                        }
                        hayFantasma = true;
                        break;
                    }
                }

                if (!hayFantasma) {
                    if (pacman.x == x && pacman.y == y) {
                        SetConsoleTextAttribute(hConsole, 14); // Amarillo
                        cout << PACMAN;
                    }
                    else {
                        char tile = laberinto[y][x];
                        switch (tile) {
                        case PARED:
                            SetConsoleTextAttribute(hConsole, COLOR_NEON_BLUE); // Azul neón
                            cout << PARED;
                            break;
                        case PUNTO:
                            SetConsoleTextAttribute(hConsole, 7); // Blanco
                            cout << PUNTO;
                            break;
                        case POWER_PELLET:
                            SetConsoleTextAttribute(hConsole, 13); // Magenta
                            cout << POWER_PELLET;
                            break;
                        default:
                            SetConsoleTextAttribute(hConsole, 7); // Blanco
                            cout << VACIO;
                            break;
                        }
                    }
                }
            }
            cout << endl;
        }

        // Reset color
        SetConsoleTextAttribute(hConsole, 7);

        cout << "Controles: Flechas/WASD(mover), Q(salir), R(reiniciar)           " << endl;

        if (!juegoActivo) {
            cout << "GAME OVER! Puntuacion final: " << puntuacion << "                         " << endl;
        }
    }

    void actualizar(Direccion direccionPacman) {
        if (!juegoActivo) return;

        moverPacman(direccionPacman);
        moverFantasmas();
        verificarColisiones();
        actualizarPowerMode();
    }

    bool estaActivo() const {
        return juegoActivo;
    }
};

// Función para detectar teclas presionadas
bool isKeyPressed(int key) {
    return (GetAsyncKeyState(key) & 0x8000) != 0;
}

Direccion obtenerDireccionDesdeTeclado() {
    if (isKeyPressed(VK_UP) || isKeyPressed('W')) return ARRIBA;
    if (isKeyPressed(VK_DOWN) || isKeyPressed('S')) return ABAJO;
    if (isKeyPressed(VK_LEFT) || isKeyPressed('A')) return IZQUIERDA;
    if (isKeyPressed(VK_RIGHT) || isKeyPressed('D')) return DERECHA;
    return NINGUNA;
}

int main() {
    PacmanGame juego;

    // Configuración de la ventana de consola
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(hConsole, &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    // Limpiar pantalla una vez al inicio
    system("cls");

    // Configuración del juego
    int velocidad = 100; // milisegundos entre updates
    auto ultimoUpdate = chrono::steady_clock::now();
    auto ultimoInput = chrono::steady_clock::now();
    int inputDelay = 80; // delay entre inputs

    cout << "BIENVENIDO A PACMAN!" << endl;
    cout << "Recolecta todos los puntos y evita a los fantasmas!" << endl;
    cout << "Los power pellets te permiten comer fantasmas temporalmente." << endl;
    cout << "Presiona cualquier tecla para comenzar..." << endl;
    _getch();

    Direccion direccionActual = DERECHA;

    while (juego.estaActivo()) {
        auto ahora = chrono::steady_clock::now();
        auto tiempoTranscurrido = chrono::duration_cast<chrono::milliseconds>(ahora - ultimoUpdate).count();
        auto tiempoInput = chrono::duration_cast<chrono::milliseconds>(ahora - ultimoInput).count();

        // Procesar input
        if (tiempoInput > inputDelay) {
            Direccion nuevaDireccion = obtenerDireccionDesdeTeclado();
            if (nuevaDireccion != NINGUNA) {
                direccionActual = nuevaDireccion;
                ultimoInput = ahora;
            }

            if (isKeyPressed('Q')) {
                break;
            }

            // Tecla R: reiniciar juego (una pulsación)
            if (isKeyPressed('R')) {
                juego.reiniciarJuego();
                ultimoInput = ahora; // evita múltiples reinicios por mantener la tecla
            }
        }

        // Actualizar juego (a ritmo fijado por 'velocidad')
        if (tiempoTranscurrido > velocidad) {
            juego.actualizar(direccionActual);
            ultimoUpdate = ahora;
        }

        // Dibujar (rápido: reposicionar cursor en lugar de clear completo)
        juego.dibujar();

        // Mantener latencia baja sin consumir CPU al 100%
        this_thread::sleep_for(chrono::milliseconds(16)); // ~60 FPS de render
    }

    // Pantalla final
    juego.dibujar();
    cout << "Presiona cualquier tecla para salir..." << endl;
    _getch();

    // Restaurar cursor
    cursorInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);

    return 0;
}