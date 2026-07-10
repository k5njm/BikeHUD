import CoreLocation
import Foundation

/// GNSS feed for speed / distance / elevation / accuracy (hub-side sensors).
@MainActor
final class LocationTelemetry: NSObject, ObservableObject {
    @Published private(set) var speedMps: Double?
    @Published private(set) var distanceMeters: Double = 0
    @Published private(set) var elevationMeters: Double?
    @Published private(set) var horizontalAccuracyMeters: Double?
    @Published private(set) var authorization: CLAuthorizationStatus = .notDetermined
    @Published private(set) var lastError: String?

    private let manager = CLLocationManager()
    private var lastLocation: CLLocation?
    private var tracking = false

    override init() {
        super.init()
        manager.delegate = self
        manager.desiredAccuracy = kCLLocationAccuracyBest
        manager.activityType = .fitness
        manager.distanceFilter = 1
        manager.pausesLocationUpdatesAutomatically = false
        authorization = manager.authorizationStatus
    }

    func requestPermission() {
        manager.requestWhenInUseAuthorization()
    }

    func start() {
        tracking = true
        distanceMeters = 0
        lastLocation = nil
        switch manager.authorizationStatus {
        case .authorizedAlways, .authorizedWhenInUse:
            manager.startUpdatingLocation()
        case .notDetermined:
            manager.requestWhenInUseAuthorization()
        default:
            lastError = "Location permission denied"
        }
    }

    func stop() {
        tracking = false
        manager.stopUpdatingLocation()
    }

    /// Instantaneous speed in cm/s for the wire packet (0 if invalid).
    var speedCmPerSec: UInt16 {
        guard let s = speedMps, s >= 0, s.isFinite else { return 0 }
        let cm = s * 100
        return UInt16(min(max(cm, 0), Double(UInt16.max)))
    }

    var distanceMetersU16: UInt16 {
        UInt16(min(max(distanceMeters, 0), Double(UInt16.max)))
    }

    var elevationMetersI16: Int16 {
        guard let e = elevationMeters, e.isFinite else { return 0 }
        let clamped = min(max(e, Double(Int16.min)), Double(Int16.max))
        return Int16(clamped)
    }

    var gpsAccuracyU8: UInt8 {
        guard let a = horizontalAccuracyMeters, a >= 0, a.isFinite else {
            return 0xFF
        }
        return UInt8(min(a, 254))
    }

    var hasFix: Bool {
        guard let a = horizontalAccuracyMeters else { return false }
        return a >= 0 && a < 50
    }
}

extension LocationTelemetry: CLLocationManagerDelegate {
    nonisolated func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        Task { @MainActor in
            self.authorization = manager.authorizationStatus
            if self.tracking {
                switch manager.authorizationStatus {
                case .authorizedAlways, .authorizedWhenInUse:
                    manager.startUpdatingLocation()
                default:
                    break
                }
            }
        }
    }

    nonisolated func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        guard let loc = locations.last else { return }
        Task { @MainActor in
            if let prev = self.lastLocation, self.tracking {
                let delta = loc.distance(from: prev)
                // Ignore GPS jumps
                if delta > 0, delta < 100, loc.horizontalAccuracy >= 0,
                   loc.horizontalAccuracy < 40
                {
                    self.distanceMeters += delta
                }
            }
            self.lastLocation = loc

            if loc.speed >= 0 {
                self.speedMps = loc.speed
            }
            if loc.verticalAccuracy >= 0 {
                self.elevationMeters = loc.altitude
            }
            self.horizontalAccuracyMeters = loc.horizontalAccuracy
        }
    }

    nonisolated func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        Task { @MainActor in
            self.lastError = error.localizedDescription
        }
    }
}
