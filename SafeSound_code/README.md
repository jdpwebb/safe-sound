# Safe Sound Embedded Software

The Safe Sound embedded software runs a machine learning model on the Microsoft Azure Sphere, and sends any events up to Azure Cloud Services. Configuration for the device is communicated using the device twin stored in the Azure IoT Hub.

# Safe Sound Hardware

The Azure Sphere was chosen for the Safe Sound device because of its focus on security both in the silicon and in the custom OS designed for defense against IoT threats. As a carrier for the chip, the [Avnet Azure Sphere Starter Kit](https://www.avnet.com/shop/us/products/avid-technologies/aes-ms-mt3620-sk-g-3074457345636825680/?aka_re=1) is used.

The Safe Sound device also requires a microphone hooked up to ADC 1 (the click slot #1 on the Starter Kit).

# Installation

Installing the code on the Azure Sphere requires first installing the Azure Sphere SDK, claiming the device, and configuring networking. See the [documentation](https://docs.microsoft.com/en-us/azure-sphere/install/overview) for instructions on how to do this.

Enable development on the Sphere by following [these instructions](https://docs.microsoft.com/en-us/azure-sphere/reference/azsphere-device#enable-development-edv).

Finally, load the code using the [directions here](https://docs.microsoft.com/en-us/azure-sphere/app-development/manual-build) or open the project in Visual Studio 2019 via File > Open > CMake... and selecting the CMakeLists.txt file. Additionally, there is a Visual Studio Code extension for the Azure Sphere in preview at this moment.

## Connecting to Azure IoT Hub

To use Azure Cloud Services, you will need to commission a Device Provisioning Service and an Azure IoT Hub. Then follow [these directions](https://github.com/Azure/azure-sphere-samples/blob/master/Samples/AzureIoT/IoTHub.md#configure-the-sample-application-to-work-with-your-azure-iot-hub) to link the Sphere to the Cloud Services.

# Acknowledgements

The [Embedded Learning Library](https://github.com/microsoft/ELL) developed by Microsoft is used to run the machine learning models on the Azure Sphere.