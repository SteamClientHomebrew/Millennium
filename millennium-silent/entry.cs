using System;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace millennium_silent
{
    internal static class entry
    {
        [STAThread]
        static async Task Main()
        {
            Console.WriteLine("[Millennium Installer]");

            if (!user.get_parameters().Any())
            {
                Console.WriteLine(" [fatal] this application isn't supposed to be ran directly");
                Console.WriteLine(" [+] startup args [\"install\"(bool)] and [\"skin\"(string)] need to be provided");
                return;
            }
       
            try
            {
                if (user.should_install())
                {
                    Console.WriteLine($"adding desired skin to user settings [{user.fetch_desired_skin()}]");
                    steam.set_skin();

                    Console.WriteLine("downloading required files...");
                    await installer.install_millennium();
                }
                else
                {
                    Console.WriteLine("uninstalling millennium...");
                    installer.uninstall_millennium();
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine(ex.ToString());
            }
        }
    }
}
