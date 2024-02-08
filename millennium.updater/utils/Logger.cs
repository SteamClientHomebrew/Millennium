using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
internal class Logger
{
    private static readonly string LogFilePath = "updater.log";
    public static void LogToConsole(string message, bool dontWriteLine)
    {
        if (dontWriteLine)
        {
            Console.Write(message);
            return;
        }
        Console.WriteLine(message);
    }
    public static void LogToFile(string message, bool dontWriteLine)
    {
        try
        {
            using (StreamWriter writer = File.AppendText(LogFilePath))
            {
                if (dontWriteLine)
                {
                    writer.Write(message);
                }
                else
                {
                    writer.WriteLine(message);
                }
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error writing to log file: {ex.Message}");
        }
    }
    public static void Log(string message, ConsoleColor color = ConsoleColor.White, bool dontWriteLine = false)
    {
        Console.ForegroundColor = color;
        LogToConsole(message, dontWriteLine);
        Console.ResetColor();

        LogToFile(message, dontWriteLine);
    }

    public static void Log(string message, bool dontWriteLine)
    {
        LogToConsole(message, dontWriteLine);
        LogToFile(message, dontWriteLine);
    }
}
