package demo;

public class Shell {
    public static void main(String[] args) {
        while (true) {
            io.Console.print("> ");
            String command = io.Console.input();
            if (utils.StringUtils.equals(command, "exit")) {
                io.Console.println("");
                break;
            }
            execute(command);
        }
    }

    static void execute(String command) {
        if (utils.StringUtils.equals(command, "help")) {
            help();
        } else if (utils.StringUtils.equals(command, "dir")) {
            dir();
        } else if (utils.StringUtils.startsWith(command, "cd ")) {
            cd(utils.StringUtils.substring(command, 3));
        } else {
            unknown(command);
        }
    }

    static void help() {
        io.Console.println("Available commands: help, dir, cd <path>, exit");
    }

    static void dir() {
        String name = io.File.findFirst("*.*");
        while (name != null) {
            io.Console.println(name);
            name = io.File.findNext();
        }
    }

    static void cd(String path) {
        if (!io.File.changeDir(path)) {
            io.Console.println("Invalid directory");
        }
    }

    static void unknown(String command) {
        if (!utils.StringUtils.isEmpty(command)) {
            io.Console.print("Unknown command:");
            io.Console.print(command);
            io.Console.println("");
        }
        io.Console.println("Type 'help' for a list of commands");
    }
}
