"use client";

import { useEffect, useState } from 'react';
import { Activity, Power, PowerOff, IndianRupee, Zap, MapPin, QrCode } from 'lucide-react';

export default function Dashboard() {
  const [data, setData] = useState<any>(null);
  const [loading, setLoading] = useState(true);

  const fetchData = async () => {
    try {
      const res = await fetch('/api/meters');
      const json = await res.json();
      setData(json);
    } catch (e) {
      console.error(e);
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    fetchData();
    const interval = setInterval(fetchData, 5000);
    return () => clearInterval(interval);
  }, []);

  if (loading && !data) {
    return <div className="min-h-screen flex items-center justify-center bg-gray-50 text-gray-900">Loading Dashboard...</div>;
  }

  return (
    <div className="min-h-screen bg-gray-50 text-gray-900 font-sans p-6">
      <div className="max-w-6xl mx-auto space-y-8">
        
        {/* Header */}
        <header className="flex justify-between items-end pb-4 border-b border-gray-200">
          <div>
            <h1 className="text-3xl font-bold tracking-tight">{data?.organization?.name}</h1>
            <p className="text-sm text-gray-500 mt-1">Lampitex Operator Dashboard</p>
          </div>
          <div className="text-right">
            <span className="inline-flex items-center gap-1.5 px-3 py-1 rounded-full text-sm font-medium bg-green-100 text-green-800">
              <span className="w-2 h-2 rounded-full bg-green-500 animate-pulse"></span>
              Live Sync Active
            </span>
          </div>
        </header>

        {/* Meters Grid */}
        <main className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
          {data?.meters?.map((meter: any) => {
            const isRelayOn = meter.relay_on;
            const bal = parseFloat(meter.balance);
            const isLowBal = bal < 50 && bal > 0;
            const isCutoff = bal <= 0;
            const reading = meter.latest_reading;

            return (
              <div key={meter.id} className="bg-white rounded-2xl shadow-sm border border-gray-100 overflow-hidden hover:shadow-md transition-shadow">
                
                {/* Card Header */}
                <div className="p-5 border-b border-gray-50 flex justify-between items-start">
                  <div>
                    <h2 className="text-lg font-bold flex items-center gap-2">
                      {meter.label}
                      <span className={`inline-flex px-2 py-0.5 rounded text-xs font-semibold ${
                        meter.is_online ? 'bg-blue-100 text-blue-700' : 'bg-gray-200 text-gray-700'
                      }`}>
                        {meter.is_online ? 'ONLINE' : 'OFFLINE'}
                      </span>
                      <span className={`inline-flex px-2 py-0.5 rounded text-xs font-semibold ${
                        isRelayOn ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'
                      }`}>
                        {isRelayOn ? 'POWER ON' : 'CUTOFF'}
                      </span>
                    </h2>
                    <p className="text-xs text-gray-500 mt-1 flex items-center gap-1">
                      <MapPin className="w-3 h-3" /> {meter.property_name}
                    </p>
                  </div>
                  {/* QR Code Icon / ID */}
                  <div className="text-right flex flex-col items-end">
                    <QrCode className="w-6 h-6 text-indigo-500 mb-1" />
                    <span className="text-[10px] font-mono text-gray-400 bg-gray-100 px-1.5 rounded">{meter.qr_code}</span>
                  </div>
                </div>

                {/* Card Body */}
                <div className="p-5 space-y-5">
                  {/* Balance Section */}
                  <div className={`p-4 rounded-xl flex items-center justify-between ${
                    isCutoff ? 'bg-red-50 text-red-900 border border-red-100' :
                    isLowBal ? 'bg-amber-50 text-amber-900 border border-amber-100' :
                    'bg-indigo-50 text-indigo-900 border border-indigo-100'
                  }`}>
                    <div>
                      <p className="text-xs font-medium uppercase tracking-wider opacity-70">Prepaid Balance</p>
                      <h3 className="text-2xl font-black flex items-center mt-0.5">
                        <IndianRupee className="w-5 h-5 mr-0.5 stroke-[2.5]" />
                        {bal.toFixed(2)}
                      </h3>
                    </div>
                    {isCutoff && <PowerOff className="w-8 h-8 opacity-50" />}
                    {!isCutoff && <IndianRupee className="w-8 h-8 opacity-20" />}
                  </div>

                  {/* Telemetry Section */}
                  <div>
                    <p className="text-xs font-medium text-gray-400 uppercase tracking-wider mb-3">Live Telemetry</p>
                    {reading ? (
                      <div className="grid grid-cols-2 gap-y-4 gap-x-2">
                        <div>
                          <p className="text-xs text-gray-500">Voltage</p>
                          <p className="text-sm font-semibold text-gray-900">{reading.voltage} V</p>
                        </div>
                        <div>
                          <p className="text-xs text-gray-500">Current</p>
                          <p className="text-sm font-semibold text-gray-900">{reading.current} A</p>
                        </div>
                        <div>
                          <p className="text-xs text-gray-500">Active Power</p>
                          <p className="text-sm font-semibold text-gray-900 flex items-center gap-1">
                            <Activity className="w-3.5 h-3.5 text-blue-500" />
                            {reading.power} W
                          </p>
                        </div>
                        <div>
                          <p className="text-xs text-gray-500">Total Energy</p>
                          <p className="text-sm font-semibold text-gray-900 flex items-center gap-1">
                            <Zap className="w-3.5 h-3.5 text-yellow-500" />
                            {reading.energy_kwh} kWh
                          </p>
                        </div>
                      </div>
                    ) : (
                      <p className="text-sm text-gray-500 italic">No telemetry data yet.</p>
                    )}
                  </div>
                </div>

                {/* Card Footer */}
                <div className="px-5 py-3 bg-gray-50 border-t border-gray-100 flex justify-between items-center text-xs text-gray-500">
                  <span className="font-mono text-[10px]">{meter.device_id}</span>
                  {reading && (
                    <span>Last updated: {new Date(reading.recorded_at).toLocaleTimeString()}</span>
                  )}
                </div>

                {/* Send LCD Message & Recharge */}
                <div className="px-5 py-3 border-t border-gray-100 flex flex-col gap-2">
                  <div className="flex gap-2">
                    <input 
                      type="text" 
                      placeholder="Send LCD message..." 
                      maxLength={16}
                      className="flex-1 text-sm px-3 py-1.5 border border-gray-200 rounded focus:outline-none focus:border-indigo-500"
                      onKeyDown={(e) => {
                        if (e.key === 'Enter') {
                          const val = e.currentTarget.value;
                          if (val) {
                            fetch('/api/meters/message', {
                              method: 'POST',
                              headers: { 'Content-Type': 'application/json' },
                              body: JSON.stringify({ meter_id: meter.id, message: val })
                            });
                            e.currentTarget.value = '';
                          }
                        }
                      }}
                    />
                  </div>
                  <div className="flex gap-2">
                    <input 
                      id={`recharge-${meter.id}`}
                      type="number" 
                      placeholder="Recharge/Deduct (Rs)" 
                      className="flex-1 text-sm px-3 py-1.5 border border-gray-200 rounded focus:outline-none focus:border-green-500"
                      onKeyDown={(e) => {
                        if (e.key === 'Enter') {
                          const val = parseFloat(e.currentTarget.value);
                          if (!isNaN(val)) {
                            fetch('/api/meters/recharge', {
                              method: 'POST',
                              headers: { 'Content-Type': 'application/json' },
                              body: JSON.stringify({ meter_id: meter.id, amount: val })
                            });
                            e.currentTarget.value = '';
                          }
                        }
                      }}
                    />
                    <button 
                      onClick={() => {
                        const input = document.getElementById(`recharge-${meter.id}`) as HTMLInputElement;
                        if (input) {
                          const val = parseFloat(input.value);
                          if (!isNaN(val)) {
                            fetch('/api/meters/recharge', {
                              method: 'POST',
                              headers: { 'Content-Type': 'application/json' },
                              body: JSON.stringify({ meter_id: meter.id, amount: val })
                            });
                            input.value = '';
                          }
                        }
                      }}
                      className="bg-green-600 hover:bg-green-700 text-white text-xs font-bold px-3 py-1.5 rounded">
                      Adjust Funds
                    </button>
                  </div>
                </div>

              </div>
            );
          })}
        </main>
      </div>
    </div>
  );
}
