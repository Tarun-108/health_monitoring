'use client'

import { useEffect, useState } from 'react'
import axios from 'axios'
import { Button } from '@/components/ui/button'
import { Card, CardContent, CardHeader, CardTitle } from '@/components/ui/card'
import { Input } from '@/components/ui/input'
import { Label } from '@/components/ui/label'
import { Tabs, TabsContent, TabsList, TabsTrigger } from '@/components/ui/tabs'
import { ScrollArea } from '@/components/ui/scroll-area'
import { Alert, AlertDescription, AlertTitle } from '@/components/ui/alert'
import { LineChart, Line, XAxis, YAxis, Tooltip, ResponsiveContainer, Legend } from 'recharts'

// Utility function to convert UTC to IST
const convertToIST = (utcTimestamp: string) => {
  const utcTime = new Date(utcTimestamp);
  const date = new Date(utcTime.getTime() + (5.5 * 60 * 60 * 1000));
  return date.toLocaleString('en-US', { 
    timeZone: 'Asia/Kolkata',
    hour12: false,
    year: 'numeric',
    month: '2-digit',
    day: '2-digit',
    hour: '2-digit',
    minute: '2-digit',
    second: '2-digit'
  }).replace(',', '');
}

type SensorData = {
  bpm: number
  bpm_avg: number
  ds18b20_temp: number
  dht11_temp: number
  humidity: number
  timestamp: string
}

export default function HealthDashboard() {
  const [data, setData] = useState<SensorData>()
  const [history, setHistory] = useState<SensorData[]>([])
  const [currentPage, setCurrentPage] = useState(1)
  const [totalPages, setTotalPages] = useState(1)
  const pageSize = 15
  const [ssid, setSsid] = useState('')
  const [password, setPassword] = useState('')
  const [message, setMessage] = useState('')
  const [showConfig, setShowConfig] = useState(false)
  const [loading, setLoading] = useState(true)
  const [diagnosis, setDiagnosis] = useState('')
  const [isDiagnosisLoading, setIsDiagnosisLoading] = useState(false)
  const [symptoms, setSymptoms] = useState('')

  const fetchData = async () => {
    try {
      const res = await axios.get('https://healthmonitoring-production.up.railway.app/sensor/data')
      setData(res.data)
    } catch (err) {
      console.error('Failed to fetch current sensor data', err)
    } finally {
      setLoading(false)
    }
  }

  const fetchHistory = async (page: number) => {
    try {
      const res = await axios.get(`https://healthmonitoring-production.up.railway.app/sensor/history?page=${page}&page_size=${pageSize}`)
      setHistory(res.data.data)
      setTotalPages(res.data.totalPages)
    } catch (err) {
      console.error('Failed to fetch history', err)
    }
  }

  const fetchDiagnosis = async () => {
    if (!data) return
    setIsDiagnosisLoading(true)
    
    const heartRate = data.bpm_avg
    const bodyTempC = data.ds18b20_temp
    const roomTemp = data.dht11_temp
    const humidity = data.humidity

    try {
      const res = await axios.post(
        'https://openrouter.ai/api/v1/chat/completions',
        {
          model: 'openai/gpt-3.5-turbo',
          messages: [
              {
                role: 'system',
                content:
                  'You are a helpful medical assistant. and tell me the condition the patient might be in and take consideration of surrounding under 100 words',
              },
              {
                role: 'user',
                content: `Patient has a heart rate of ${heartRate} bpm and a body temperature of ${bodyTempC.toFixed(1)}°C and currently the room temperature is ${roomTemp.toFixed(1)}°C and humidity is ${humidity.toFixed(1)}%. ${symptoms ? `Patient reported symptoms: ${symptoms}` : 'Patient reported no symptoms.'} What is the condition?`,
              },
            ],
          temperature: 0.7,
          max_tokens: 100
        },
        {
          headers: {
            'Authorization': `Bearer ${process.env.NEXT_PUBLIC_OPENROUTER_API_KEY}`,
            'Content-Type': 'application/json'
          }
        }
      )

      setDiagnosis(res.data.choices[0].message.content.trim())
    } catch (err) {
      console.error(err)
      setDiagnosis('❌ Failed to get diagnosis.')
    } finally {
      setIsDiagnosisLoading(false)
    }
  }

  useEffect(() => {
    fetchData()
    const interval = setInterval(fetchData, 5000)
    return () => clearInterval(interval)
  }, [])

  useEffect(() => {
    fetchHistory(currentPage)
  }, [currentPage])

  const handleConfigSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    try {
      await axios.post('https://healthmonitoring-production.up.railway.app/sensor/configure', { ssid, password })
      setMessage('✅ Sensor configured successfully')
    } catch (err) {
      console.error('Configuration failed', err)
      setMessage('❌ Configuration failed')
    }
  }

  const renderAlert = () => {
    if (!data) return null
    const isAbnormalBPM = data.bpm_avg < 60 || data.bpm_avg > 100
    const isAbnormalTemp = data.ds18b20_temp < 36.2 || data.ds18b20_temp > 37.5

    if (isAbnormalBPM || isAbnormalTemp) {
      return (
        <Alert variant="destructive" className="mb-4">
          <AlertTitle>⚠️ Abnormal Readings Detected</AlertTitle>
          <AlertDescription>
            {isAbnormalBPM && (
              <div>Average BPM is <b>{Math.round(data.bpm_avg)}</b> (Normal range: 60-100). Please consult a doctor if this persists.</div>
            )}
            {isAbnormalTemp && (
              <div>Average Body Temperature is <b>{data.ds18b20_temp.toFixed(1)}°C</b> (Normal range: 36.1-37.2°C). Please consult a doctor if this persists.</div>
            )}
          </AlertDescription>
        </Alert>
      )
    }

    return (
      <Alert variant="default" className="mb-4 bg-green-50 border-green-200">
        <AlertTitle className="text-green-700">✅ All Readings Normal</AlertTitle>
        <AlertDescription className="text-green-600">
          <div>Your vital signs are within normal ranges:</div>
          <div>• Heart Rate: <b>{Math.round(data.bpm_avg)} bpm</b> (Normal range: 60-100)</div>
          <div>• Body Temperature: <b>{data.ds18b20_temp.toFixed(1)}°C</b> (Normal range: 36.1-37.2°C)</div>
          <div className="mt-1">Continue monitoring your health regularly.</div>
        </AlertDescription>
      </Alert>
    )
  }

  const formattedHistory = history.map((item) => ({
    timestamp: convertToIST(item.timestamp).split(' ')[1],
    bpm: Math.round(item.bpm),
    ds18b20_temp: parseFloat(item.ds18b20_temp.toFixed(1)),
    dht11_temp: parseFloat(item.dht11_temp.toFixed(1)),
    humidity: parseFloat(item.humidity.toFixed(1)),
  }))

  return (
    <div className="max-w-5xl mx-auto py-8 px-4">
      <div className="flex justify-between items-center mb-4">
        <h1 className="text-4xl font-bold text-center flex-grow">🩺 Health Monitoring Dashboard</h1>
        <Button className="ml-4" onClick={() => setShowConfig(!showConfig)}>
          {showConfig ? 'Close' : 'Configure'}
        </Button>
      </div>

      {showConfig && (
        <Card className="mb-6">
          <CardHeader>
            <CardTitle>🔧 Configure Sensor Wi-Fi</CardTitle>
          </CardHeader>
          <CardContent className="p-6">
            <form className="space-y-4" onSubmit={handleConfigSubmit}>
              <div>
                <Label htmlFor="ssid">SSID</Label>
                <Input id="ssid" value={ssid} onChange={(e) => setSsid(e.target.value)} />
              </div>
              <div>
                <Label htmlFor="password">Password</Label>
                <Input id="password" type="password" value={password} onChange={(e) => setPassword(e.target.value)} />
              </div>
              <Button type="submit">Submit</Button>
            </form>
            {message && <p className="mt-4 text-sm font-medium text-green-600">{message}</p>}
          </CardContent>
        </Card>
      )}

      <Tabs defaultValue="current">
        <TabsList className="grid w-full grid-cols-2 mb-4">
          <TabsTrigger value="current">Live Data</TabsTrigger>
          <TabsTrigger value="history">History</TabsTrigger>
        </TabsList>

        <TabsContent value="current">
          {loading ? (
            <div className="text-center py-12 text-gray-500">Loading sensor data...</div>
          ) : (
            <>
              {renderAlert()}
              <Card>
                <CardHeader>
                  <CardTitle>📡 Live Sensor Data</CardTitle>
                </CardHeader>
                {data ? (
                  <CardContent className="grid gap-4 p-6">
                    <div className="text-xl">❤️ Heart Rate: <b>{Math.round(data.bpm)} bpm</b></div>
                    <div className="text-xl">📊 Avg BPM: <b>{Math.round(data.bpm_avg)} bpm</b></div>
                    <div className="text-xl">🌡️ Body Temp: <b>{data.ds18b20_temp.toFixed(1)} °C</b></div>
                    <div className="text-xl">🏠 Room Temp: <b>{data.dht11_temp.toFixed(1)} °C</b></div>
                    <div className="text-xl">💧 Humidity: <b>{data.humidity.toFixed(1)}%</b></div>
                    <div className="text-sm text-gray-500">Updated: {data.timestamp ? convertToIST(data.timestamp) : ''}</div>
                  </CardContent>
                ) : (
                  <div className="p-6 text-red-500 font-medium"><b>⚠️ Data Invalid or Not Loaded</b></div>
                )}
              </Card>

              <Card className="mt-4">
                <CardHeader className="flex flex-row items-center justify-between">
                  <CardTitle>🤖 AI Diagnosis</CardTitle>
                  <Button 
                    onClick={fetchDiagnosis} 
                    disabled={isDiagnosisLoading || !data}
                    className="flex items-center gap-2"
                  >
                    {isDiagnosisLoading ? (
                      <>
                        <span className="animate-spin">🔄</span> Analyzing...
                      </>
                    ) : (
                      <>
                        <span>🔄</span> Get Diagnosis
                      </>
                    )}
                  </Button>
                </CardHeader>
                <CardContent className="p-6 space-y-4">
                  <div className="space-y-2">
                    <Label htmlFor="symptoms">Additional Symptoms (Optional)</Label>
                    <Input 
                      id="symptoms" 
                      placeholder="Enter any symptoms you're experiencing..." 
                      value={symptoms}
                      onChange={(e) => setSymptoms(e.target.value)}
                    />
                  </div>
                  {diagnosis ? (
                    <div className="text-lg">{diagnosis}</div>
                  ) : (
                    <div className="text-gray-500 italic">
                      Click the button above to get an AI diagnosis based on your current sensor readings.
                    </div>
                  )}
                </CardContent>
              </Card>
            </>
          )}
        </TabsContent>

        <TabsContent value="history">
          <Tabs defaultValue="bpm">
            <TabsList className="grid w-full grid-cols-3 mb-4">
              <TabsTrigger value="bpm">BPM</TabsTrigger>
              <TabsTrigger value="temp">Body Temp</TabsTrigger>
              <TabsTrigger value="env">Room Temp & Humidity</TabsTrigger>
            </TabsList>

            <TabsContent value="bpm">
              <Card>
                <CardHeader>
                  <CardTitle>📈 BPM Over Time</CardTitle>
                </CardHeader>
                <CardContent className="h-72">
                  <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={[...formattedHistory].reverse()}>
                      <XAxis dataKey="timestamp" tick={{ fontSize: 10 }} />
                      <YAxis domain={[40, 160]} tick={{ fontSize: 10 }} />
                      <Tooltip />
                      <Line type="monotone" dataKey="bpm" stroke="#ef4444" strokeWidth={2} dot={false} />
                    </LineChart>
                  </ResponsiveContainer>
                </CardContent>
              </Card>
            </TabsContent>

            <TabsContent value="temp">
              <Card>
                <CardHeader>
                  <CardTitle>🌡️ Body Temperature Over Time</CardTitle>
                </CardHeader>
                <CardContent className="h-72">
                  <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={[...formattedHistory].reverse()}>
                      <XAxis dataKey="timestamp" tick={{ fontSize: 10 }} />
                      <YAxis domain={['auto', 'auto']} tick={{ fontSize: 10 }} />
                      <Tooltip />
                      <Line type="monotone" dataKey="ds18b20_temp" stroke="#f97316" strokeWidth={2} dot={false} />
                    </LineChart>
                  </ResponsiveContainer>
                </CardContent>
              </Card>
            </TabsContent>

            <TabsContent value="env">
              <Card>
                <CardHeader>
                  <CardTitle>🏠 Room Temp & 💧 Humidity Over Time</CardTitle>
                </CardHeader>
                <CardContent className="h-72">
                  <ResponsiveContainer width="100%" height="100%">
                    <LineChart data={[...formattedHistory].reverse()}>
                      <XAxis dataKey="timestamp" tick={{ fontSize: 10 }} />
                      <YAxis domain={['auto', 'auto']} tick={{ fontSize: 10 }} />
                      <Tooltip />
                      <Legend />
                      <Line type="monotone" dataKey="dht11_temp" stroke="#3b82f6" strokeWidth={2} dot={false} name="Room Temp" />
                      <Line type="monotone" dataKey="humidity" stroke="#10b981" strokeWidth={2} dot={false} name="Humidity" />
                    </LineChart>
                  </ResponsiveContainer>
                </CardContent>
              </Card>
            </TabsContent>
          </Tabs>

          <Card className="mt-6">
            <CardHeader>
              <CardTitle>🕑 Historical Readings</CardTitle>
            </CardHeader>
            <CardContent>
              <ScrollArea className="h-64 border rounded-lg p-4">
                <ul className="space-y-2 text-sm">
                  {formattedHistory.map((item, index) => (
                    <li key={index} className="flex justify-between">
                      <span>❤️ {item.bpm} bpm</span>
                      <span>🌡️ {item.ds18b20_temp} °C</span>
                      <span>🏠 {item.dht11_temp} °C</span>
                      <span>💧 {item.humidity}%</span>
                      <span>{item.timestamp}</span>
                    </li>
                  ))}
                </ul>
              </ScrollArea>
              <div className="flex justify-center items-center gap-2 mt-4">
                <Button 
                  variant="outline" 
                  onClick={() => setCurrentPage(prev => Math.max(1, prev - 1))}
                  disabled={currentPage === 1}
                >
                  Previous
                </Button>
                <span className="text-sm">
                  Page {currentPage} of {totalPages}
                </span>
                <Button 
                  variant="outline" 
                  onClick={() => setCurrentPage(prev => Math.min(totalPages, prev + 1))}
                  disabled={currentPage === totalPages}
                >
                  Next
                </Button>
              </div>
            </CardContent>
          </Card>
        </TabsContent>
      </Tabs>
    </div>
  )
}
