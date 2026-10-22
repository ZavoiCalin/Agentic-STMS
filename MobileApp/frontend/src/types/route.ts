export type RoutePoint = {
  lat: number;
  lng: number;
};

export type RouteSummary = {
  routeId: string;
  status?: string;
  startedAt?: string;
  endedAt?: string;
  distanceKm?: number;
};

export type RouteResponse = {
  route: RouteSummary & {
    coordinates: RoutePoint[];
  };
};

export type MapCoordinate = {
  latitude: number;
  longitude: number;
};
