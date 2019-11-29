import 'dart:convert';
import 'package:meta/meta.dart';
import 'package:crypto/crypto.dart';
import 'package:http/http.dart' as http;

/// Specifies which section to update on the device twin properties.
enum TwinUpdateSection {
  /// Requests an update of the twin desired properties.
  desired,

  /// Requests an update of the twin reported properties.
  reported
}

class AzureIoTHub {
  /// API Version for communication with Azure IoT Hubs
  static const API_VERSION = "2018-06-30";

  static const DEFAULT_SAS_TTL = Duration(hours: 1);

  /// Name of the Azure IoT Hub.
  final String iotHubEndpoint;

  /// Shared Access Key for the requested policy.
  final String sharedAccessKey;

  /// Policy name to use to connect.
  final String policy;

  String _sas;
  DateTime _sasExpirationTime;
  Map<String, String> _defaultHeaders;

  /// Also generates and stores a Shared Access String (SAS) for use in
  /// future calls.
  AzureIoTHub({
    @required this.iotHubEndpoint,
    @required this.sharedAccessKey,
    this.policy = "service",
  })  : assert(iotHubEndpoint != null),
        assert(sharedAccessKey != null) {
    refreshSAS();
  }

  /// Generates a Shared Access String using the provided URI, Shared Access Key, policy and time-to-live.
  static String generateSASToken({
    @required String uri,
    @required String key,
    String policyName,
    Duration ttl = const Duration(hours: 1),
  }) {
    var encodedURI = Uri.encodeComponent(uri);
    var expiryTime = DateTime.now().add(ttl);
    var expiryTimeInSeconds =
        (expiryTime.millisecondsSinceEpoch / Duration.millisecondsPerSecond)
            .toStringAsFixed(0);
    var stringToSign = "$encodedURI\n$expiryTimeInSeconds";
    var hmacSha256 = new Hmac(sha256, base64.decode(key));
    var signature = hmacSha256.convert(utf8.encode(stringToSign));
    var signatureEncoded = Uri.encodeComponent(base64.encode(signature.bytes));

    var token =
        "SharedAccessSignature sr=$encodedURI&sig=$signatureEncoded&se=$expiryTimeInSeconds";
    if (policyName != null) {
      token += "&skn=" + policyName;
    }
    return token;
  }
  
  /// Refreshes the SAS token.
  /// 
  /// Useful if the SAS token has expired.
  void refreshSAS() {
    _sas = generateSASToken(
      uri: this.iotHubEndpoint,
      key: this.sharedAccessKey,
      policyName: this.policy,
      ttl: DEFAULT_SAS_TTL,
    );
    _sasExpirationTime = DateTime.now().add(DEFAULT_SAS_TTL);
    _defaultHeaders = {
      "Authorization": _sas,
      "Content-Type": "application/json"
    };
  }

  void _prepareForRequest() {
    if (DateTime.now().isAfter(this._sasExpirationTime)) {
      refreshSAS();
    }
  }

  /// Post processes the response object.
  /// 
  /// If the request for the given response was successful, the response body is returned.
  /// Otherwise, an Exception is thrown with a message that varies depending on the error.
  String _processResponse({@required http.Response response}) {
    if (response.statusCode >= 200 && response.statusCode < 300) {
      return response.body;
    } else {
      // If that response was not OK, throw an error.
      // returns 404 for devices not connected, 401 for auth error
      var message = response.reasonPhrase;
      if (response.statusCode == 404) {
      message = "Device not online.";
      }
      else if (response.statusCode == 401) {
        message = "Unauthorized. Check that the Shared Access Key is correct.";
      }
      throw(Exception(message));
    }
  }

  /// Call the direct method specified by [methodName] on the device with [deviceID].
  /// Returns the response from the device.
  Future<String> callDirectMethod({
    @required String deviceID,
    @required String methodName,
    String payload,
    Duration responseTimeout = const Duration(seconds: 20),
  }) async {
    _prepareForRequest();
    var uri =
        "https://$iotHubEndpoint/twins/$deviceID/methods?api-version=$API_VERSION";

    Map<String, dynamic> bodyProps = {
      "methodName": methodName,
      "responseTimeoutInSeconds": responseTimeout.inSeconds,
    };
    if (payload != null) {
      bodyProps["payload"] = payload;
    }
    var body = jsonEncode(bodyProps);
    final response = await http.post(uri, headers: _defaultHeaders, body: body);

    return _processResponse(response: response);
  }

  Future<Map<String, dynamic>> retrieveDeviceTwin({@required String deviceID}) async {
    _prepareForRequest();
    var uri = "https://$iotHubEndpoint/twins/$deviceID?api-version=$API_VERSION";
    final response = await http.get(uri, headers: _defaultHeaders);
    return jsonDecode(_processResponse(response: response));
  }

  /// Updates the twin properties specified in [updates].
  ///
  /// Returns the device twin on success.
  /// Throws an HttpException on error.
  Future<Map<String, dynamic>> updateDeviceTwin({
    /// Alphanumeric device ID string assigned in Azure IoT Hub.
    @required String deviceID,

    /// Properties to update arranged as key-value pairs that can be encoded as a JSON object.
    @required Map<String, dynamic> updates,

    /// Specifies whether to update the reported or desired properties.
    TwinUpdateSection updateSection = TwinUpdateSection.desired,
  }) async {
    _prepareForRequest();
    var uri = "https://$iotHubEndpoint/twins/$deviceID?api-version=$API_VERSION";

    var sectionToUpdate = "desired";
    if (updateSection == TwinUpdateSection.reported) {
      sectionToUpdate = "reported";
    }
    var updateProps = {
      "properties": {
        sectionToUpdate: updates,
      }
    };

    var body = jsonEncode(updateProps);
    final response = await http.patch(
      uri,
      headers: _defaultHeaders,
      body: body,
    );

    return jsonDecode(_processResponse(response: response));
  }
}
