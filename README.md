# java86

Interpréteur minimal de bytecode Java 1.4 pour FreeDOS 16-bit via V86.js + Node.js.

## Démarrage rapide

```bash
npm install
npm run start
```

Affichage interactif FreeDOS avec redirection série. Les commandes DOS fonctionnent.

## Architecture

- **V86 + FreeDOS**: Émulateur x86 16-bit en Node.js
- **Redirection série**: CTTY COM1 → stdout/stdin Node
- **Compilation C**: gcc-ia16 dans FreeDOS
- **VM bytecode**: À implémenter

## Contraintes

- Méthodes statiques uniquement
- Pas d'objets/interfaces/long/double
- Classes lues depuis ZIP sans compression
- Recherche des classes dans les JAR indiqués par `-classpath`, séparés par `;`
- Natives via `platform.Native`
- Exécutable < 64K NEAR

## Scripts

- `npm run start` — Lance FreeDOS avec TTY série et copie les `.COM` de `release/` ainsi que les `.JAR` de `build/` à la racine du disque C:
- `npm run emulator` — Sert FreeDOS dans le navigateur (écran VGA + clavier) sur http://localhost:8086/
- `npm run build -- src/java.c` — Compile les sources Java avec Janino, crée un JAR sans compression par package (ex. `build/DEMO.JAR`, `build/PLATFORM.JAR`), puis compile uniquement le fichier C indiqué
- `npm run install:dev` — Télécharge dépendances

Dans FreeDOS : `java -classpath DEMO.JAR;PLATFORM.JAR demo.Hello`. Les JAR restent ouverts pendant l'exécution ; le cache de dix entrées utilise le CRC16 du nom et ne détecte pas les collisions.

## État

✅ V86 + FreeDOS + redirection série fonctionne
⏳ gcc-ia16 setup, Java compiler, bytecode interpreter
