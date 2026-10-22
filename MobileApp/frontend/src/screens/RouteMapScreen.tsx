import React, { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  Pressable,
  SafeAreaView,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import MapView, { Marker, Polyline, Region } from 'react-native-maps';
import { fetchLatestRoute } from '../services/routeApi';
import { MapCoordinate, RoutePoint, RouteSummary } from '../types/route';

function convertToMapCoordinates(points: RoutePoint[]): MapCoordinate[] {
  return points.map((point) => ({
    latitude: Number(point.lat),
    longitude: Number(point.lng),
  }));
}

function formatDistance(distanceKm?: number): string {
  if (distanceKm === undefined || Number.isNaN(distanceKm)) {
    return '—';
  }

  return `${distanceKm.toFixed(1)} km`;
}

export default function RouteMapScreen() {
  const mapRef = useRef<MapView | null>(null);
  const [route, setRoute] = useState<RoutePoint[]>([]);
  const [routeMeta, setRouteMeta] = useState<RouteSummary | null>(null);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const latestRouteRef = useRef<RoutePoint[]>([]);
  const latestRouteMetaRef = useRef<RouteSummary | null>(null);

  useEffect(() => {
    latestRouteRef.current = route;
  }, [route]);

  useEffect(() => {
    latestRouteMetaRef.current = routeMeta;
  }, [routeMeta]);

  const loadRoute = useCallback(async () => {
    const currentRoute = latestRouteRef.current;
    const currentMeta = latestRouteMetaRef.current;

    try {
      const result = await fetchLatestRoute();
      const nextRoute = result?.route?.coordinates?.length ? result.route.coordinates : currentRoute;

      setRoute(nextRoute);
      setRouteMeta({
        routeId: result?.route?.routeId || currentMeta?.routeId || 'route-offline',
        status: result?.route?.status || (nextRoute.length ? currentMeta?.status || 'in_progress' : 'offline'),
        startedAt: result?.route?.startedAt,
        endedAt: result?.route?.endedAt,
        distanceKm: result?.route?.distanceKm ?? currentMeta?.distanceKm,
      });
    } catch (error) {
      console.error('Route fetch failed', error);
      Alert.alert('Route Error', 'Unable to fetch route from backend. Showing last known route.');
      if (currentRoute.length) {
        setRoute(currentRoute);
      }
      setRouteMeta({
        routeId: currentMeta?.routeId || 'route-offline',
        status: 'offline',
        startedAt: currentMeta?.startedAt,
        endedAt: currentMeta?.endedAt,
        distanceKm: currentMeta?.distanceKm,
      });
    } finally {
      setLoading(false);
      setRefreshing(false);
    }
  }, []);

  useEffect(() => {
    let intervalId: ReturnType<typeof setInterval> | undefined;

    loadRoute();
    intervalId = setInterval(() => {
      loadRoute();
    }, 15000);

    return () => {
      if (intervalId) {
        clearInterval(intervalId);
      }
    };
  }, [loadRoute]);

  const mapCoordinates = useMemo(() => convertToMapCoordinates(route), [route]);

  useEffect(() => {
    if (mapRef.current && mapCoordinates.length > 1) {
      mapRef.current.fitToCoordinates(mapCoordinates, {
        edgePadding: { top: 140, right: 50, bottom: 120, left: 50 },
        animated: true,
      });
    }
  }, [mapCoordinates]);

  const handleRefresh = () => {
    setRefreshing(true);
    loadRoute();
  };

  const initialRegion: Region = mapCoordinates.length
    ? {
        latitude: mapCoordinates[0].latitude,
        longitude: mapCoordinates[0].longitude,
        latitudeDelta: 0.02,
        longitudeDelta: 0.02,
      }
    : {
        latitude: 45.7489,
        longitude: 21.2087,
        latitudeDelta: 0.05,
        longitudeDelta: 0.05,
      };

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.header}>
        <View style={styles.headerRow}>
          <View style={styles.headerTextWrap}>
            <Text style={styles.title}>Traffic Route</Text>
            <Text style={styles.subtitle}>Live route map</Text>
          </View>

          <Pressable style={styles.refreshButton} onPress={handleRefresh}>
            <Text style={styles.refreshText}>{refreshing ? 'Refreshing...' : 'Refresh'}</Text>
          </Pressable>
        </View>
      </View>

      <View style={styles.summaryCard}>
        <Text style={styles.summaryLabel}>Route</Text>
        <Text style={styles.summaryValue}>{routeMeta?.routeId || 'Loading...'}</Text>
        <View style={styles.summaryRow}>
          <Text style={styles.summaryMetric}>Status: {routeMeta?.status || 'loading'}</Text>
          <Text style={styles.summaryMetric}>Distance: {formatDistance(routeMeta?.distanceKm)}</Text>
        </View>
      </View>

      {loading ? (
        <View style={styles.loaderContainer}>
          <ActivityIndicator size="large" color="#2563eb" />
          <Text style={styles.loaderText}>Loading route map...</Text>
        </View>
      ) : (
        <MapView
          ref={mapRef}
          style={styles.map}
          initialRegion={initialRegion}
          showsCompass
          showsScale
          showsTraffic
          showsBuildings
          toolbarEnabled
          provider="google"
          mapType="standard"
        >
          {mapCoordinates.length > 1 && (
            <Polyline
              coordinates={mapCoordinates}
              strokeColor="#2563eb"
              strokeWidth={6}
              lineJoin="round"
              lineCap="round"
            />
          )}

          {mapCoordinates.length > 0 && (
            <Marker coordinate={mapCoordinates[0]} title="Start" pinColor="#16a34a" />
          )}

          {mapCoordinates.length > 1 && (
            <Marker coordinate={mapCoordinates[mapCoordinates.length - 1]} title="Destination" pinColor="#dc2626" />
          )}
        </MapView>
      )}
    </SafeAreaView>
  );
}

const styles = StyleSheet.create({
  safeArea: {
    flex: 1,
    backgroundColor: '#f8fafc',
  },
  header: {
    paddingHorizontal: 16,
    paddingTop: 16,
    paddingBottom: 8,
    backgroundColor: '#ffffff',
    borderBottomWidth: 1,
    borderBottomColor: '#e2e8f0',
  },
  headerRow: {
    flexDirection: 'row',
    alignItems: 'center',
    justifyContent: 'space-between',
  },
  headerTextWrap: {
    flex: 1,
    paddingRight: 8,
  },
  title: {
    fontSize: 24,
    fontWeight: '700',
    color: '#0f172a',
  },
  subtitle: {
    marginTop: 4,
    fontSize: 14,
    color: '#475569',
  },
  refreshButton: {
    paddingHorizontal: 14,
    paddingVertical: 8,
    backgroundColor: '#2563eb',
    borderRadius: 10,
  },
  refreshText: {
    color: '#ffffff',
    fontWeight: '600',
    fontSize: 13,
  },
  summaryCard: {
    marginHorizontal: 16,
    marginTop: 12,
    marginBottom: 8,
    padding: 14,
    backgroundColor: '#ffffff',
    borderRadius: 14,
    shadowColor: '#0f172a',
    shadowOffset: { width: 0, height: 3 },
    shadowOpacity: 0.08,
    shadowRadius: 10,
    elevation: 4,
  },
  summaryLabel: {
    fontSize: 12,
    textTransform: 'uppercase',
    letterSpacing: 0.8,
    color: '#64748b',
    marginBottom: 4,
  },
  summaryValue: {
    fontSize: 18,
    fontWeight: '700',
    color: '#0f172a',
  },
  summaryRow: {
    marginTop: 10,
    flexDirection: 'row',
    justifyContent: 'space-between',
    gap: 12,
  },
  summaryMetric: {
    fontSize: 13,
    color: '#334155',
    fontWeight: '600',
  },
  loaderContainer: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    backgroundColor: '#f8fafc',
  },
  loaderText: {
    marginTop: 12,
    fontSize: 16,
    color: '#334155',
  },
  map: {
    flex: 1,
    marginHorizontal: 0,
  },
});
