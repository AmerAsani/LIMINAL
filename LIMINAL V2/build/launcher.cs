// LIMINAL.exe - Starter fuer LIMINAL V2
//
// Startet die mitgelieferte Python-Laufzeit (Ordner "runtime") mit dem Spiel im
// selben Konsolenfenster. Es wird keine Python-Installation und keine
// Entwicklungsumgebung benoetigt. Kompiliert mit dem in Windows enthaltenen
// C#-Compiler (siehe build_exe.py).

using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Text;

[assembly: AssemblyTitle("LIMINAL")]
[assembly: AssemblyDescription("LIMINAL - unendliche 3D-Innenraeume im Terminal")]
[assembly: AssemblyProduct("LIMINAL")]
[assembly: AssemblyCompany("LIMINAL")]
[assembly: AssemblyCopyright("LIMINAL V2")]
[assembly: AssemblyVersion("2.0.0.0")]
[assembly: AssemblyFileVersion("2.0.0.0")]

static class Program
{
    static int Main(string[] args)
    {
        string baseDir = AppDomain.CurrentDomain.BaseDirectory;
        string python = Path.Combine(baseDir, Path.Combine("runtime", "python.exe"));
        string script = Path.Combine(baseDir, "liminal3d.py");
        try { Console.Title = "LIMINAL"; } catch (Exception) { }

        if (!File.Exists(python) || !File.Exists(script))
        {
            Console.WriteLine("LIMINAL: Neben LIMINAL.exe fehlen der Ordner 'runtime' oder 'liminal3d.py'.");
            Console.WriteLine("Bitte den kompletten Ordner 'LIMINAL V2' behalten. Fuer den Desktop am besten");
            Console.WriteLine("eine Verknuepfung auf LIMINAL.exe anlegen, statt die EXE allein zu verschieben.");
            Pause();
            return 1;
        }

        ProcessStartInfo psi = new ProcessStartInfo(python);
        psi.Arguments = "-X utf8 -B " + Quote(script) + " " + JoinArgs(args);
        psi.UseShellExecute = false;           // gleiche Konsole weiterverwenden
        psi.WorkingDirectory = baseDir;
        psi.EnvironmentVariables["LIMINAL_LAUNCHER"] = "1";
        psi.EnvironmentVariables.Remove("PYTHONHOME");
        psi.EnvironmentVariables.Remove("PYTHONPATH");

        // Strg+C beendet das Spiel selbst - der Starter wartet einfach
        Console.CancelKeyPress += delegate (object s, ConsoleCancelEventArgs e) { e.Cancel = true; };

        try
        {
            using (Process p = Process.Start(psi))
            {
                p.WaitForExit();
                if (p.ExitCode != 0)
                {
                    Console.WriteLine();
                    Console.WriteLine("LIMINAL wurde mit Fehlercode " + p.ExitCode + " beendet.");
                    Pause();
                }
                return p.ExitCode;
            }
        }
        catch (Exception ex)
        {
            Console.WriteLine("LIMINAL konnte nicht gestartet werden: " + ex.Message);
            Pause();
            return 1;
        }
    }

    static void Pause()
    {
        Console.WriteLine("Taste druecken zum Schliessen ...");
        try { Console.ReadKey(true); } catch (Exception) { }
    }

    static string Quote(string s)
    {
        return "\"" + s.Replace("\"", "\\\"") + "\"";
    }

    static string JoinArgs(string[] args)
    {
        StringBuilder sb = new StringBuilder();
        foreach (string a in args)
        {
            if (sb.Length > 0) sb.Append(' ');
            sb.Append(a.IndexOfAny(new char[] { ' ', '\t', '"' }) >= 0 ? Quote(a) : a);
        }
        return sb.ToString();
    }
}
