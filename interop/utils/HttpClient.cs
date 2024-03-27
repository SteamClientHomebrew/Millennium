using System;
using System.Collections.Generic;
using System.Linq;
using System.Net.Http;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace updater
{
    internal class Http
    {
        private static void HandleError(string message)
        {
            Logger.Log($"Error making GET request: {message}");

            if (message.Contains("403 (rate limit exceeded)"))
            {
                MessageBox.Show("Github API has a rate limit of 60rph which you have succeeded. Please wait at max an hour and retry", "Updater - Rate Limited", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }
        public static async Task<string> Get(string url)
        {
            using (HttpClient client = new HttpClient())
            {

                client.DefaultRequestHeaders.UserAgent.ParseAdd("Millennium.Updater");

                try
                {
                    HttpResponseMessage response = await client.GetAsync(url);
                    response.EnsureSuccessStatusCode();

                    return await response.Content.ReadAsStringAsync();
                }
                catch (HttpRequestException ex)
                {
                    HandleError(ex.Message);
                }
                catch (Exception ex)
                {
                    HandleError(ex.Message);
                }
            }
            return string.Empty;
        }
    }
}
