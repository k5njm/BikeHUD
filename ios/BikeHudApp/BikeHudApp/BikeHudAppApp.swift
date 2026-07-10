import SwiftUI

@main
struct BikeHudAppApp: App {
    @StateObject private var rideController = RideController()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(rideController)
        }
    }
}
