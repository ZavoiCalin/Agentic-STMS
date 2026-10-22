import React, { useCallback, useEffect, useMemo, useState } from 'react';
import {
  ActivityIndicator,
  Alert,
  Linking,
  Pressable,
  SafeAreaView,
  ScrollView,
  StyleSheet,
  Text,
  View,
} from 'react-native';
import { fetchLatestRoute } from '../services/routeApi';
import { RoutePoint, RouteSummary } from '../types/route';

function formatDistance(distanceKm?: number): string {
  if (distanceKm === undefined || Number.isNaN(distanceKm)) {
    return '—';
  }

  return `${distanceKm.toFixed(1)} km`;
}

export default function RouteMapScreen() {
  const [route, setRoute] = useState<RoutePoint[]>([]);
  const [mapRoute, setMapRoute] = useState<RoutePoint[]>([]);
  const [routeMeta, setRouteMeta] = useState<RouteSummary | null>(null);
  const [loading, setLoading] = useState(true);
  const [refreshing, setRefreshing] = useState(false);
  const lastMapUpdateRef = React.useRef(0);
  const latestRouteRef = React.useRef<RoutePoint[]>([]);
  const latestRouteMetaRef = React.useRef<RouteSummary | null>(null);

  useEffect(() => {
    latestRouteRef.current = route;
  }, [route]);

  useEffect(() => {
    latestRouteMetaRef.current = routeMeta;
  }, [routeMeta]);

  const loadRoute = useCallback(async (forceMapUpdate = false) => {
    const currentRoute = latestRouteRef.current;
    const currentMeta = latestRouteMetaRef.current;

    try {
      const result = await fetchLatestRoute();
      const nextRoute = result?.route?.coordinates?.length ? result.route.coordinates : currentRoute;
      const now = Date.now();
      const shouldUpdateMap =
        forceMapUpdate || !mapRoute.length || now - lastMapUpdateRef.current >= 8000;

      setRoute(nextRoute);
      if (shouldUpdateMap) {
        setMapRoute(nextRoute);
        lastMapUpdateRef.current = now;
      }
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
      if (!mapRoute.length && currentRoute.length) {
        setMapRoute(currentRoute);
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
  }, [mapRoute.length]);

  useEffect(() => {
    let intervalId: ReturnType<typeof setInterval> | undefined;

    loadRoute();
    intervalId = setInterval(() => {
      loadRoute();
    }, 4000);

    return () => {
      if (intervalId) {
        clearInterval(intervalId);
      }
    };
  }, [loadRoute]);

  const navigationUrl = useMemo(() => {
    if (!route.length) {
      return 'https://www.google.com/maps';
    }

    if (route.length === 1) {
      const singlePoint = route[0];
      return `https://www.google.com/maps/dir/?api=1&destination=${singlePoint.lat},${singlePoint.lng}&travelmode=driving&dir_action=navigate`;
    }

    const start = route[0];
    const destination = route[route.length - 1];
    const waypoints = route
      .slice(1, route.length - 1)
      .map((point) => `${point.lat},${point.lng}`)
      .join('|');

    let nextUrl = `https://www.google.com/maps/dir/?api=1&origin=${start.lat},${start.lng}&destination=${destination.lat},${destination.lng}&travelmode=driving&dir_action=navigate`;
    if (waypoints) {
      nextUrl += `&waypoints=${encodeURIComponent(waypoints)}`;
    }

    return nextUrl;
  }, [route]);

  const navigationCtaLabel = 'Start route in Google Maps';

  const embeddedMapUrl = useMemo(() => {
    if (!mapRoute.length) {
      return 'https://maps.google.com/maps?output=embed&f=d&saddr=45.7489,21.2087&daddr=45.7633,21.2301&dirflg=d';
    }

    if (mapRoute.length === 1) {
      const singlePoint = mapRoute[0];
      return `https://maps.google.com/maps?output=embed&q=${singlePoint.lat},${singlePoint.lng}&z=14`;
    }

    const start = mapRoute[0];
    const restOfRoute = mapRoute
      .slice(1)
      .map((point) => `${point.lat},${point.lng}`)
      .join('+to:');

    return `https://maps.google.com/maps?output=embed&f=d&saddr=${start.lat},${start.lng}&daddr=${restOfRoute}&dirflg=d`;
  }, [mapRoute]);

  const handleRefresh = () => {
    setRefreshing(true);
    loadRoute(true);
  };

  return (
    <SafeAreaView style={styles.safeArea}>
      <View style={styles.header}>
        <View style={styles.headerRow}>
          <View style={styles.headerTextWrap}>
            <Text style={styles.title}>Traffic Route</Text>
            <Text style={styles.subtitle}>Live route data (web)</Text>
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
          <Text style={styles.loaderText}>Loading route data...</Text>
        </View>
      ) : (
        <ScrollView contentContainerStyle={styles.content}>
          <Text style={styles.sectionTitle}>Live map</Text>
          <View style={styles.mapFrameContainer}>
            {React.createElement('iframe', {
              title: 'STMS route map',
              src: embeddedMapUrl,
              loading: 'lazy',
              allowFullScreen: true,
              referrerPolicy: 'no-referrer-when-downgrade',
              style: styles.mapFrame as unknown as Record<string, unknown>,
            })}
          </View>

          <Text style={styles.sectionTitle}>Route points ({route.length})</Text>
          {route.map((point, index) => (
            <Text key={`${point.lat}-${point.lng}-${index}`} style={styles.pointText}>
              {index + 1}. {point.lat.toFixed(6)}, {point.lng.toFixed(6)}
            </Text>
          ))}

          <Pressable onPress={() => Linking.openURL(navigationUrl)}>
            <Text style={styles.mapLink}>{navigationCtaLabel}</Text>
          </Pressable>
          <Text style={styles.helperText}>The embedded map is a preview only. Open Google Maps to start the turn-by-turn navigation view with the live blue-arrow guidance.</Text>
        </ScrollView>
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
  },
  loaderContainer: {
    flex: 1,
    alignItems: 'center',
    justifyContent: 'center',
  },
  loaderText: {
    marginTop: 12,
    color: '#475569',
    fontSize: 14,
  },
  content: {
    paddingHorizontal: 16,
    paddingBottom: 24,
  },
  sectionTitle: {
    fontSize: 16,
    fontWeight: '700',
    color: '#0f172a',
    marginBottom: 10,
    marginTop: 8,
  },
  mapFrameContainer: {
    width: '100%',
    aspectRatio: 16 / 9,
    minHeight: 240,
    maxHeight: 460,
    borderRadius: 14,
    overflow: 'hidden',
    borderWidth: 1,
    borderColor: '#e2e8f0',
    backgroundColor: '#ffffff',
    marginBottom: 8,
  },
  mapFrame: {
    width: '100%',
    height: '100%',
    borderWidth: 0,
  },
  pointText: {
    fontSize: 13,
    color: '#1f2937',
    marginBottom: 6,
  },
   mapLink: {
     marginTop: 16,
     color: '#2563eb',
     fontWeight: '600',
     textDecorationLine: 'underline',
   },
   helperText: {
     marginTop: 8,
     color: '#475569',
     fontSize: 12,
     lineHeight: 18,
   },
 });
