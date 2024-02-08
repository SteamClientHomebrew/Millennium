using System;
using System.Net.Http;
using System.Threading.Tasks;
using System.IO;
using System.Net;
using System.Net.Security;

namespace updater.builder
{
    internal class DownloadFileAsync
    {
        public DownloadFileAsync() { }


        public static async Task<bool> Download(string url, string path)
        {
            try
            {
                // Bypass SSL/TLS certificate validation
                using (var handler = new HttpClientHandler())
                {

                    handler.ServerCertificateCustomValidationCallback += (sender, certificate, chain, sslPolicyErrors) => {
                        if (sslPolicyErrors != SslPolicyErrors.None)
                        {
                            Logger.Log($"SSL/TLS Error: {sslPolicyErrors}");
                            // Log or handle the error appropriately
                        }
                        return true; // Allow all certificates (for troubleshooting purposes)
                    };

                    using (var client = new HttpClient(handler))
                    {
                        // Download the file
                        var content = await client.GetByteArrayAsync(url);

                        // Save the file
                        File.WriteAllBytes(path, content);

                        return true;
                    }
                }
            }
            catch (Exception ex)
            {
                Logger.Log($"Error making GET request: {ex.Message}");
                return false;
            }
        }

    }
}
