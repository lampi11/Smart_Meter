"use client";

import { useEffect, useState } from 'react';
import { Activity, PowerOff, IndianRupee, Zap, MapPin, QrCode, Plus, Trash2, ShieldCheck, RefreshCw, Wifi } from 'lucide-react';

export default function Dashboard() {
  const [data, setData] = useState<any>(null);
  const [loading, setLoading] = useState(true);
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [claimForm, setClaimForm] = useState({ serial: '', code: '', label: '' });
  const [claimStatus, setClaimStatus] = useState<{ loading: boolean; error: string; success: string }>({
    loading: false,
    error: '',
    success: ''
  });

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
    const interval = setInterval(fetchData, 1500);
    return () => clearInterval(interval);
  }, []);

  const handleClaimMeter = async (e: React.FormEvent) => {
    e.preventDefault();
    setClaimStatus({ loading: true, error: '', success: '' });

    try {
      const res = await fetch('/api/meters/claim', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          serial_number: claimForm.serial,
          claim_code: claimForm.code,
          label: claimForm.label
        })
      });

      const result = await res.json();
      if (!res.ok) {
        setClaimStatus({ loading: false, error: result.error || 'Failed to claim meter', success: '' });
      } else {
        setClaimStatus({ loading: false, error: '', success: 'Meter verified & linked successfully!' });
        setTimeout(() => {
          setIsModalOpen(false);
          setClaimForm({ serial: '', code: '', label: '' });
          setClaimStatus({ loading: false, error: '', success: '' });
          fetchData();
        }, 1200);
      }
    } catch (err: any) {
      setClaimStatus({ loading: false, error: 'Network error claiming meter', success: '' });
    }
  };

  const handleDeleteMeter = async (meterId: string, label: string) => {
    if (!confirm(`Are you sure you want to remove ${label} from your dashboard?`)) return;

    try {
      await fetch(`/api/meters?id=${meterId}`, { method: 'DELETE' });
      fetchData();
    } catch (e) {
      console.error('Delete meter error:', e);
    }
  };

  const handleAdjustFunds = async (meterId: string, amount: number) => {
    // Instant optimistic update on UI (< 5ms)
    setData((prev: any) => {
      if (!prev || !prev.meters) return prev;
      return {
        ...prev,
        meters: prev.meters.map((m: any) => {
          if (m.id === meterId) {
            const currentBal = parseFloat(m.balance) || 0;
            return {
              ...m,
              balance: (currentBal + amount).toFixed(2)
            };
          }
          return m;
        })
      };
    });

    try {
      const res = await fetch('/api/meters/recharge', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ meter_id: meterId, amount })
      });
      const result = await res.json();
      if (result.success) {
        await fetchData();
      } else {
        alert(result.error || 'Failed to adjust funds');
        await fetchData(); // rollback
      }
    } catch (err) {
      console.error('Adjust funds error:', err);
      await fetchData(); // rollback
    }
  };

  const handleSendMessage = async (meterId: string, message: string) => {
    try {
      const res = await fetch('/api/meters/message', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ meter_id: meterId, message })
      });
      const d = await res.json();
      if (d.success) {
        alert(`Message "${message}" queued for meter screen!`);
      }
    } catch (err) {
      console.error('Send message error:', err);
    }
  };

  if (loading && !data) {
    return (
      <div className="min-h-screen flex items-center justify-center bg-gray-50 text-gray-700">
        <RefreshCw className="w-5 h-5 animate-spin mr-2 text-indigo-600" /> Loading Dashboard...
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-gray-50 text-gray-900 font-sans p-6">
      <div className="max-w-6xl mx-auto space-y-8">
        
        {/* Header */}
        <header className="flex flex-col md:flex-row justify-between items-start md:items-end gap-4 pb-4 border-b border-gray-200">
          <div>
            <h1 className="text-3xl font-extrabold tracking-tight text-gray-900">
              {data?.organization?.name || 'Lampitex Smart Meter Network'}
            </h1>
            <p className="text-sm text-gray-500 mt-1">Multi-Tenant Prepaid Energy Management System</p>
          </div>
          <div className="flex items-center gap-3">
            <span className="inline-flex items-center gap-1.5 px-3 py-1.5 rounded-full text-xs font-semibold bg-emerald-50 text-emerald-700 border border-emerald-200">
              <span className="w-2 h-2 rounded-full bg-emerald-500 animate-pulse"></span>
              Cloud Live Sync
            </span>
            <button
              onClick={() => setIsModalOpen(true)}
              className="inline-flex items-center gap-1.5 px-4 py-2 rounded-lg text-sm font-semibold bg-indigo-600 text-white hover:bg-indigo-700 transition shadow-sm"
            >
              <Plus className="w-4 h-4" /> Add / Claim Meter
            </button>
          </div>
        </header>

        {/* Meters Grid or Empty State */}
        {(!data?.meters || data.meters.length === 0) ? (
          <div className="text-center py-20 bg-white rounded-2xl border border-dashed border-gray-200 p-8 shadow-sm">
            <ShieldCheck className="w-12 h-12 text-indigo-400 mx-auto mb-3" />
            <h3 className="text-lg font-bold text-gray-800">No meters connected yet</h3>
            <p className="text-sm text-gray-500 max-w-md mx-auto mt-1 mb-6">
              Connect your manufactured meter using its Serial Number and the 6-digit setup code shown on the screen.
            </p>
            <button
              onClick={() => setIsModalOpen(true)}
              className="inline-flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-bold bg-indigo-600 text-white hover:bg-indigo-700"
            >
              <Plus className="w-4 h-4" /> Claim Your First Meter
            </button>
          </div>
        ) : (
          <main className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
            {data?.meters?.map((meter: any) => {
              const isRelayOn = meter.relay_on;
              const bal = parseFloat(meter.balance);
              const isLowBal = bal < 50 && bal > 0;
              const isCutoff = bal <= 0;
              const reading = meter.latest_reading;

              return (
                <div key={meter.id} className="bg-white rounded-2xl shadow-sm border border-gray-100 overflow-hidden hover:shadow-md transition-shadow flex flex-col justify-between">
                  
                  {/* Card Header */}
                  <div>
                    <div className="p-5 border-b border-gray-50 flex justify-between items-start">
                      <div>
                        <h2 className="text-lg font-bold flex items-center gap-2 text-gray-900">
                          {meter.label}
                          <span className={`inline-flex px-2 py-0.5 rounded text-[11px] font-semibold ${
                            meter.is_online ? 'bg-blue-100 text-blue-700' : 'bg-gray-200 text-gray-700'
                          }`}>
                            {meter.is_online ? 'ONLINE' : 'OFFLINE'}
                          </span>
                          <span className={`inline-flex px-2 py-0.5 rounded text-[11px] font-semibold ${
                            isRelayOn ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'
                          }`}>
                            {isRelayOn ? 'POWER ON' : 'CUTOFF'}
                          </span>
                        </h2>
                        <p className="text-xs text-gray-500 mt-1 flex items-center gap-1">
                          <MapPin className="w-3 h-3" /> {meter.property_name}
                        </p>
                      </div>

                      <div className="flex flex-col items-end gap-1.5">
                        <button
                          onClick={() => handleDeleteMeter(meter.id, meter.label)}
                          className="text-gray-300 hover:text-red-500 transition p-1"
                          title="Remove meter from dashboard"
                        >
                          <Trash2 className="w-4 h-4" />
                        </button>
                        <span className="text-[10px] font-mono text-gray-400 bg-gray-50 px-1.5 py-0.5 rounded border border-gray-100">
                          {meter.serial_number || meter.device_id}
                        </span>
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
                  </div>

                  {/* Card Controls */}
                  <div>
                    {/* Device info footer */}
                    <div className="px-5 py-2.5 bg-gray-50 border-t border-gray-100 flex justify-between items-center text-xs text-gray-500 font-mono">
                      <span>{meter.device_id}</span>
                      <span>
                        Sync: {meter.last_seen_at ? new Date(meter.last_seen_at).toLocaleTimeString() : 'Never'}
                      </span>
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
                              const val = e.currentTarget.value.trim();
                              if (val) {
                                handleSendMessage(meter.id, val);
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
                          step="any"
                          placeholder="Recharge/Deduct (Rs)" 
                          className="flex-1 text-sm px-3 py-1.5 border border-gray-200 rounded focus:outline-none focus:border-green-500 font-semibold"
                          onKeyDown={(e) => {
                            if (e.key === 'Enter') {
                              const val = parseFloat(e.currentTarget.value);
                              if (!isNaN(val)) {
                                handleAdjustFunds(meter.id, val);
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
                                handleAdjustFunds(meter.id, val);
                                input.value = '';
                              }
                            }
                          }}
                          className="bg-green-600 hover:bg-green-700 text-white text-xs font-bold px-3 py-1.5 rounded transition">
                          Adjust Funds
                        </button>
                      </div>

                      <button
                        onClick={async () => {
                          if (!confirm(`Deploy wireless firmware update to ${meter.label}? The meter will download and install firmware.bin wirelessly.`)) return;
                          try {
                            const res = await fetch('/api/meters/ota', {
                              method: 'POST',
                              headers: { 'Content-Type': 'application/json' },
                              body: JSON.stringify({ meter_id: meter.id })
                            });
                            const d = await res.json();
                            alert(d.message || 'OTA update signal sent! The meter will begin flashing.');
                          } catch (e) {
                            alert('Failed to send OTA command');
                          }
                        }}
                        className="w-full text-center text-[11px] font-semibold text-indigo-600 hover:text-indigo-800 hover:bg-indigo-50 py-1 rounded transition flex items-center justify-center gap-1">
                        <Zap className="w-3 h-3 text-indigo-500" /> Push Wireless OTA Update
                      </button>

                      <button
                        onClick={async () => {
                          if (!confirm(`Reset Wi-Fi credentials on ${meter.label}? The meter will wipe saved Wi-Fi and reboot into setup access point Lampitex-Setup so you can enter new credentials.`)) return;
                          try {
                            const res = await fetch('/api/meters/reset-wifi', {
                              method: 'POST',
                              headers: { 'Content-Type': 'application/json' },
                              body: JSON.stringify({ meter_id: meter.id })
                            });
                            const d = await res.json();
                            alert(d.message || 'Wi-Fi reset signal sent! The meter will reboot into Setup Portal mode.');
                          } catch (e) {
                            alert('Failed to send Wi-Fi reset command');
                          }
                        }}
                        className="w-full text-center text-[11px] font-semibold text-amber-600 hover:text-amber-800 hover:bg-amber-50 py-1 rounded transition flex items-center justify-center gap-1">
                        <Wifi className="w-3 h-3 text-amber-500" /> Reset Wi-Fi via OTA
                      </button>
                    </div>
                  </div>

                </div>
              );
            })}
          </main>
        )}

      </div>

      {/* Add / Claim Meter Modal */}
      {isModalOpen && (
        <div className="fixed inset-0 bg-black/40 backdrop-blur-sm flex items-center justify-center p-4 z-50">
          <div className="bg-white rounded-2xl max-w-md w-full p-6 shadow-xl space-y-5 animate-in fade-in zoom-in-95 duration-150">
            <div className="flex justify-between items-center pb-2 border-b border-gray-100">
              <div className="flex items-center gap-2">
                <ShieldCheck className="w-5 h-5 text-indigo-600" />
                <h3 className="text-lg font-bold text-gray-900">Claim & Register Meter</h3>
              </div>
              <button
                onClick={() => setIsModalOpen(false)}
                className="text-gray-400 hover:text-gray-600 text-xl font-bold"
              >
                &times;
              </button>
            </div>

            <p className="text-xs text-gray-500">
              Enter the Serial Number printed on the meter label, along with the Security Claim Code shown during WiFi configuration.
            </p>

            <form onSubmit={handleClaimMeter} className="space-y-4">
              <div>
                <label className="block text-xs font-semibold text-gray-700 uppercase tracking-wider mb-1">
                  Serial Number
                </label>
                <input
                  type="text"
                  placeholder="e.g. LMPTX-C104"
                  value={claimForm.serial}
                  onChange={(e) => setClaimForm({ ...claimForm, serial: e.target.value })}
                  required
                  className="w-full text-sm px-3 py-2 border border-gray-200 rounded-lg focus:outline-none focus:border-indigo-600 uppercase font-mono"
                />
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-700 uppercase tracking-wider mb-1">
                  Security Claim Code
                </label>
                <input
                  type="text"
                  placeholder="6-digit setup code (e.g. 749215)"
                  value={claimForm.code}
                  onChange={(e) => setClaimForm({ ...claimForm, code: e.target.value })}
                  required
                  className="w-full text-sm px-3 py-2 border border-gray-200 rounded-lg focus:outline-none focus:border-indigo-600 font-mono"
                />
              </div>

              <div>
                <label className="block text-xs font-semibold text-gray-700 uppercase tracking-wider mb-1">
                  Meter Label / Room Name
                </label>
                <input
                  type="text"
                  placeholder="e.g. Room 101 or Main Supply"
                  value={claimForm.label}
                  onChange={(e) => setClaimForm({ ...claimForm, label: e.target.value })}
                  className="w-full text-sm px-3 py-2 border border-gray-200 rounded-lg focus:outline-none focus:border-indigo-600"
                />
              </div>

              {claimStatus.error && (
                <div className="p-3 bg-red-50 text-red-700 text-xs rounded-lg border border-red-100">
                  {claimStatus.error}
                </div>
              )}

              {claimStatus.success && (
                <div className="p-3 bg-emerald-50 text-emerald-700 text-xs rounded-lg border border-emerald-100">
                  {claimStatus.success}
                </div>
              )}

              <div className="flex justify-end gap-2 pt-2">
                <button
                  type="button"
                  onClick={() => setIsModalOpen(false)}
                  className="px-4 py-2 text-xs font-semibold text-gray-600 hover:bg-gray-100 rounded-lg"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={claimStatus.loading}
                  className="px-5 py-2 text-xs font-bold text-white bg-indigo-600 hover:bg-indigo-700 rounded-lg transition disabled:opacity-50"
                >
                  {claimStatus.loading ? 'Verifying...' : 'Verify & Add Meter'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
}
