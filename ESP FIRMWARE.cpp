
using System;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading.Tasks;
using Windows.Devices.Bluetooth;
using Windows.Devices.Bluetooth.GenericAttributeProfile;
using Windows.Devices.Enumeration;
using Windows.Storage.Streams;

class Program
{
    static readonly Guid ServiceId =
        Guid.Parse("8b7e1000-7c4a-4f3d-9a11-123456789abc");

    static readonly Guid CharacteristicId =
        Guid.Parse("8b7e1001-7c4a-4f3d-9a11-123456789abc");

    static async Task Main()
    {
        using var server = new TcpListener(IPAddress.Loopback, 8765);
        server.Start();
        Console.WriteLine("Waiting for Python on port 8765...");

        using var client = await server.AcceptTcpClientAsync();
        using var writer = new System.IO.StreamWriter(client.GetStream())
        {
            AutoFlush = true
        };

        Console.WriteLine("Python connected. Searching for S3_PIANO...");

        var devices = await DeviceInformation.FindAllAsync(
            BluetoothLEDevice.GetDeviceSelector());

        var deviceInfo = devices.FirstOrDefault(
            d => d.Name == "S3_PIANO");

        if (deviceInfo == null)
        {
            Console.WriteLine("S3_PIANO not found. Check that the ESP32 is powered on and advertising.");
            return;
        }

        using var device =
            await BluetoothLEDevice.FromIdAsync(deviceInfo.Id);

        if (device == null)
        {
            Console.WriteLine("Could not open the BLE device.");
            return;
        }

        var serviceResult =
            await device.GetGattServicesForUuidAsync(ServiceId);

        if (serviceResult.Status != GattCommunicationStatus.Success)
        {
            Console.WriteLine("Could not access the BLE service.");
            return;
        }

        var service = serviceResult.Services.FirstOrDefault();

        if (service == null)
        {
            Console.WriteLine("BLE service not found.");
            return;
        }

        var characteristicResult =
            await service.GetCharacteristicsForUuidAsync(CharacteristicId);

        var characteristic =
            characteristicResult.Characteristics.FirstOrDefault();

        if (characteristic == null)
        {
            Console.WriteLine("BLE characteristic not found.");
            return;
        }

        characteristic.ValueChanged += async (_, args) =>
        {
            var reader = DataReader.FromBuffer(args.CharacteristicValue);
            var bytes = new byte[reader.UnconsumedBufferLength];
            reader.ReadBytes(bytes);
            var message = Encoding.UTF8.GetString(bytes);

            try
            {
                await writer.WriteLineAsync(message);
                Console.WriteLine(message);
            }
            catch
            {
                Console.WriteLine("Python connection closed.");
            }
        };

        var status = await characteristic.WriteClientCharacteristicConfigurationDescriptorAsync(
            GattClientCharacteristicConfigurationDescriptorValue.Notify);

        if (status != GattCommunicationStatus.Success)
        {
            Console.WriteLine("Could not enable BLE notifications.");
            return;
        }

        Console.WriteLine("Connected to S3_PIANO. Press Ctrl+C to stop.");

        await Task.Delay(-1);
    }
}
